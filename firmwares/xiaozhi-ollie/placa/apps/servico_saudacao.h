// Serviço (sem tela na gaveta): saudação com o nome do usuário na tela de espera.
// Aparece ao ligar (depois do logo) e ao voltar para a espera, e troca a cada 15 min ao longo do dia,
// com frases do período (bom dia, boa tarde...). O Clawd troca de expressão/pose a cada 40–90 s,
// com poses que combinam com a hora do dia. Também informa ao servidor o nome do agente.
#pragma once

#include <algorithm>
#include <ctime>
#include <vector>

#include <esp_random.h>

#include "../agente_watcher.h"
#include "../nucleo_apps.h"

class ServicoSaudacao : public AppWatcher {
public:
    const char* Nome() const override { return "Saudação"; }
    bool Visivel() const override { return false; }
    void Abrir(ContextoApps& c) override {}
    bool Clicar(ContextoApps& c) override { return false; }

    void Fundo(ContextoApps& c) override {
        int agora = ContextoApps::Agora();
        SincronizarPerfil(agora);
        if (ContextoApps::App().GetDeviceState() != kDeviceStateIdle) {
            ocioso_desde_ = -1;
            return;
        }
        if (ocioso_desde_ < 0) {
            ocioso_desde_ = agora;
            proxima_ = agora + 2;  // logo depois de limpar a tela da conversa
            proxima_pose_ = agora + 20;
        }
        if (c.gaveta_aberta && c.gaveta_aberta()) {
            return;
        }
        if (agora - ContextoApps::ultimo_aviso < kTrocaS) {  // não cobre um aviso recente
            proxima_ = std::max(proxima_, agora + 60);
            return;
        }
        if (agora >= proxima_pose_) {
            proxima_pose_ = agora + 40 + (int)(esp_random() % 51);  // 40 a 90 s
            std::string pose = Pose();
            ContextoApps::App().Schedule([pose]() { Board::GetInstance().GetDisplay()->SetEmotion(pose.c_str()); });
        }
        if (agora < proxima_) {
            return;
        }
        proxima_ = agora + kTrocaS;
        proxima_pose_ = agora + 40;  // a pose nova acompanha a frase nova
        std::string pose = Pose();
        std::string frase = Frase();
        ContextoApps::App().Schedule([pose]() { Board::GetInstance().GetDisplay()->SetEmotion(pose.c_str()); });
        ContextoApps::App().Schedule([frase]() {
            Board::GetInstance().GetDisplay()->SetChatMessage("saudacao", frase.c_str());
        });
    }

private:
    static constexpr int kTrocaS = 15 * 60;
    int ocioso_desde_ = -1;
    int proxima_ = 0;
    int perfil_em_ = 0;
    bool perfil_ok_ = false;
    int ultima_frase_ = -1;
    int proxima_pose_ = 0;
    std::string ultima_pose_;

    // Pose do Clawd para a espera: um conjunto comum mais poses do período, sem repetir a anterior
    std::string Pose() {
        std::vector<const char*> poses = {"neutral", "happy", "winking", "cool", "relaxed", "waving",
                                          "confident", "thinking", "idea", "music", "nerd"};
        time_t t = time(nullptr);
        struct tm agora;
        localtime_r(&t, &agora);
        if (agora.tm_year + 1900 >= 2025) {
            int h = agora.tm_hour;
            if (h >= 5 && h < 12) {
                poses.insert(poses.end(), {"coffee", "coffee", "sunny", "running"});
            } else if (h >= 12 && h < 18) {
                poses.insert(poses.end(), {"reading", "working", "searching", "rocket"});
            } else if (h >= 18) {
                poses.insert(poses.end(), {"music", "relaxed", "party", "sleepy"});
            } else {
                poses.insert(poses.end(), {"sleepy", "sleepy", "relaxed", "coffee"});
            }
        }
        std::string escolhida = poses[esp_random() % poses.size()];
        if (escolhida == ultima_pose_) {
            escolhida = poses[(esp_random() + 1) % poses.size()];
        }
        ultima_pose_ = escolhida;
        return escolhida;
    }

    // Avisa o servidor do nome do agente e pega o nome do usuário (guardado para usar sem internet)
    void SincronizarPerfil(int agora) {
        if (perfil_ok_ || agora < perfil_em_ || !RedeWatcher::Online()) {
            return;
        }
        perfil_em_ = agora + 60;
        std::string corpo;
        if (!RedeWatcher::Pedir("GET", "/watcher/perfil?agente=" + AgenteWatcher::Nome(), "", corpo)) {
            return;
        }
        cJSON* raiz = cJSON_Parse(corpo.c_str());
        std::string usuario = RedeWatcher::Campo(raiz, "usuario");
        cJSON_Delete(raiz);
        if (!usuario.empty() && usuario != AgenteWatcher::Usuario()) {
            AgenteWatcher::DefinirUsuario(usuario);
            proxima_ = 0;  // já mostra a saudação com o nome
        }
        perfil_ok_ = true;
    }

    std::string Frase() {
        std::string nome = AgenteWatcher::Usuario();
        std::vector<std::string> frases = {
            "Olá, tudo bem, {}?", "O que vamos fazer hoje, {}?", "Como posso te ajudar, {}?",
            "Opaaa, {}, como você está?", "E aí, {}? Bora começar?", "Tô por aqui, {}. É só chamar.",
        };
        time_t t = time(nullptr);
        struct tm agora;
        localtime_r(&t, &agora);
        if (agora.tm_year + 1900 >= 2025) {  // relógio já sincronizado
            int h = agora.tm_hour;
            if (h >= 5 && h < 12) {
                frases.insert(frases.end(), {"Bom dia, {}! Café já tomado?", "Bom dia, {}! Bora começar o dia?"});
            } else if (h >= 12 && h < 18) {
                frases.insert(frases.end(), {"Boa tarde, {}! Como vai o dia?", "Boa tarde, {}. Precisa de algo?"});
            } else if (h >= 18) {
                frases.insert(frases.end(), {"Boa noite, {}! Ainda no gás?", "Boa noite, {}. Como foi o dia?"});
            } else {
                frases.insert(frases.end(), {"Opa, {}, madrugada produtiva?", "Ainda acordado, {}? Tô aqui."});
            }
        }
        int i = esp_random() % frases.size();
        if (i == ultima_frase_) {
            i = (i + 1) % frases.size();
        }
        ultima_frase_ = i;
        std::string f = frases[i];
        auto pos = f.find("{}");
        if (nome.empty()) {  // sem nome: tira o vocativo (", {}" ou "{}, ")
            if (pos >= 2 && f.compare(pos - 2, 2, ", ") == 0) {
                f.erase(pos - 2, 4);
            } else {
                f.erase(pos, f.compare(pos + 2, 2, ", ") == 0 ? 4 : 2);
            }
        } else {
            f.replace(pos, 2, nome);
        }
        return f;
    }
};

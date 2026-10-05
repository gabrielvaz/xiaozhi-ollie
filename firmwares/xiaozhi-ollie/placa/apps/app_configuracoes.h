// App "Configurações": a roda escolhe o item, o clique troca para a próxima opção (aplica e salva na hora).
#pragma once

#include <atomic>
#include <vector>

#include <esp_app_desc.h>
#include <esp_system.h>

#include "lvgl_theme.h"
#include "../abertura_watcher.h"
#include "../agente_watcher.h"
#include "../fontes_watcher.h"
#include "../nucleo_apps.h"

class AppConfiguracoes : public AppWatcher {
public:
    const char* Nome() const override { return "Ajustes"; }
    const char* Icone() const override { return MATERIAL_SYMBOLS_SETTINGS; }
    std::string Detalhe() const override { return "Nome, tema, fonte, tela…"; }

    void Abrir(ContextoApps& c) override {
        sobre_ = false;
        Desenhar(c, 0);
    }

    void Girar(ContextoApps& c, int passo) override {
        if (!sobre_) {
            c.painel.Mover(passo);
        }
    }

    bool Voltar(ContextoApps& c) override {
        if (sobre_) {
            sobre_ = false;
            Desenhar(c, kSobre);
            return true;
        }
        return false;
    }

    bool Clicar(ContextoApps& c) override {
        if (sobre_) {
            sobre_ = false;
            Desenhar(c, kSobre);
            return true;
        }
        int i = c.painel.Selecionado();
        switch (i) {
            case kAgente: {
                // Troca o nome; a palavra de ativação só muda ao reiniciar, então reinicia 8 s depois
                // do último clique (dá para girar pela lista de nomes antes)
                AgenteWatcher::Definir(AgenteWatcher::Proximo(AgenteWatcher::Nome()));
                reiniciar_em_ = ContextoApps::Agora() + 8;
                break;
            }
            case kTema: {
                std::string tema = ConfigWatcher::Texto("tema", "dark") == "dark" ? "light" : "dark";
                ConfigWatcher::SetTexto("tema", tema);
                auto* t = LvglThemeManager::GetInstance().GetTheme(tema);
                if (t != nullptr) {
                    Board::GetInstance().GetDisplay()->SetTheme(t);
                }
                break;
            }
            case kFonte: {
                Fontes::DefinirMono(!Fontes::Mono());
                // Reaplicar o tema refaz o layout da fala (sensecap_watcher.cc) já com a fonte nova
                auto* t = LvglThemeManager::GetInstance().GetTheme(ConfigWatcher::Texto("tema", "dark"));
                if (t != nullptr) {
                    Board::GetInstance().GetDisplay()->SetTheme(t);
                }
                break;
            }
            case kTela: {
                int v = Proximo(kOpcoesTela, ConfigWatcher::Int("tela_s", 60));
                ConfigWatcher::SetInt("tela_s", v);
                if (c.definir_tela_apaga_s) {
                    c.definir_tela_apaga_s(v);
                }
                break;
            }
            case kBrilho: {
                int v = Proximo(kOpcoesBrilho, Board::GetInstance().GetBacklight()->brightness());
                Board::GetInstance().GetBacklight()->SetBrightness(v, true);
                break;
            }
            case kVolume: {
                auto codec = Board::GetInstance().GetAudioCodec();
                codec->SetOutputVolume(Proximo(kOpcoesVolume, codec->output_volume()));
                break;
            }
            case kDesliga: {
                int v = Proximo(kOpcoesDesliga, ConfigWatcher::Int("desliga_s", 300));
                ConfigWatcher::SetInt("desliga_s", v);
                if (c.definir_desliga_s) {
                    c.definir_desliga_s(v);
                }
                break;
            }
            case kAvisos:
                ConfigWatcher::SetInt("avisos", ConfigWatcher::Int("avisos", 1) ? 0 : 1);
                break;
            case kSobre:
                sobre_ = true;
                MostrarSobre(c);
                return true;
            default:
                return false;  // Voltar
        }
        Desenhar(c, i);
        return true;
    }

private:
    enum { kAgente, kTema, kFonte, kTela, kBrilho, kVolume, kDesliga, kAvisos, kSobre };
    inline static const std::vector<int> kOpcoesTela = {30, 60, 120, 300, 900, -1};
    inline static const std::vector<int> kOpcoesBrilho = {25, 50, 75, 100};
    inline static const std::vector<int> kOpcoesVolume = {0, 20, 40, 60, 80, 100};
    inline static const std::vector<int> kOpcoesDesliga = {300, 900, 1800, -1};
    bool sobre_ = false;
    std::atomic<int> reiniciar_em_{0};

    static int Proximo(const std::vector<int>& opcoes, int atual) {
        for (size_t i = 0; i < opcoes.size(); i++) {
            if (opcoes[i] > atual || (opcoes[i] == -1 && atual != -1)) {
                return opcoes[i];
            }
        }
        return opcoes.front();
    }

    static std::string Tempo(int s) {
        if (s < 0) {
            return "Nunca";
        }
        return s < 60 ? std::to_string(s) + " s" : std::to_string(s / 60) + " min";
    }

    void Desenhar(ContextoApps& c, int selecionado) {
        auto& board = Board::GetInstance();
        c.painel.MostrarLista(
            "Configurações",
            {{"Nome do agente", "Hey " + AgenteWatcher::Nome() +
                                    (reiniciar_em_ ? " · reinicia em instantes" : "")},
             {"Tema", ConfigWatcher::Texto("tema", "dark") == "dark" ? "Escuro" : "Claro"},
             {"Fonte", Fontes::Nome()},
             {"Tela apaga após", Tempo(ConfigWatcher::Int("tela_s", 60))},
             {"Brilho", std::to_string(board.GetBacklight()->brightness()) + "%"},
             {"Volume", std::to_string(board.GetAudioCodec()->output_volume()) + "%"},
             {"Desliga na bateria após", Tempo(ConfigWatcher::Int("desliga_s", 300))},
             {"Avisos na tela", ConfigWatcher::Int("avisos", 1) ? "Ligados" : "Desligados"},
             {"Sobre o Watcher", "Versão, rede e cartão"},
             {"Voltar", ""}},
            selecionado);
    }

    // Reinício depois de trocar o nome do agente: avisa o servidor e reinicia para a nova ativação
    void Fundo(ContextoApps& c) override {
        int quando = reiniciar_em_;
        if (quando == 0 || ContextoApps::Agora() < quando) {
            return;
        }
        reiniciar_em_ = 0;
        std::string corpo;
        RedeWatcher::Pedir("GET", "/watcher/perfil?agente=" + AgenteWatcher::Nome(), "", corpo);
        c.painel.MostrarStatus("Nome do agente", PainelWatcher::Status::Sucesso,
                               "Agora é " + AgenteWatcher::Nome() + ". Diga “Hey " + AgenteWatcher::Nome() +
                                   "”. Reiniciando…");
        vTaskDelay(pdMS_TO_TICKS(2500));
        esp_restart();
    }

    void MostrarSobre(ContextoApps& c) {
        auto& wifi = WifiManager::GetInstance();
        std::string servidor = RedeWatcher::BaseUrl();
        auto pos = servidor.find("://");
        servidor = servidor.substr(pos == std::string::npos ? 0 : pos + 3);
        servidor = servidor.substr(0, servidor.find('/'));
        std::string texto = "Ollie v" OLLIE_VERSAO " (base XiaoZhi " + std::string(esp_app_get_description()->version) +
                            ")\nWi-Fi: " + (wifi.IsConnected() ? wifi.GetSsid() : std::string("desconectado")) +
                            "\nIP: " + (wifi.IsConnected() ? wifi.GetIpAddress() : std::string("-")) +
                            "\nMAC: " + SystemInfo::GetMacAddress() + "\nServidor: " + servidor +
                            "\nmicroSD: " + (CartaoWatcher::Instancia().Montado() ? "montado" : "ausente");
        c.painel.MostrarTexto("Sobre o Watcher", texto, {"Voltar"});
    }
};

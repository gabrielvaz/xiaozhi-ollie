// App "Reuniões": lista as reuniões gravadas (quando e duração) e grava uma nova. O áudio vai ao Mac
// (transcrição, resumo e nota) e uma cópia fica no microSD. Gravando, a roda escolhe Pausar/Continuar
// ou Parar; ao parar, volta para a lista.
// Se a conexão cair, a gravação continua só no cartão e é enviada depois (app "Cartão").
#pragma once

#include <atomic>
#include <cctype>
#include <vector>

#include "../idioma_watcher.h"
#include "../nucleo_apps.h"

class AppReuniao : public AppWatcher {
public:
    const char* Nome() const override { return TR("Gravador", "Recorder", "录音机", "Grabadora"); }
    const char* Id() const override { return "gravador"; }
    const char* Icone() const override { return MATERIAL_SYMBOLS_MIC; }
    std::string Detalhe() const override { return TR("Transcrição e resumo no Notas", "Transcript, summary in Notes", "转写与摘要存入备忘录", "Transcripción y resumen en Notas"); }
    bool PrendeTela() const override { return gravando_; }

    void Abrir(ContextoApps& c) override {
        tela_ = Tela::Carregando;
        buscar_em_ = 0;
        c.painel.MostrarStatus(TR("Gravador", "Recorder", "录音机", "Grabadora"), PainelWatcher::Status::Carregando,
                               TR("Carregando reuniões…", "Loading meetings…", "正在加载会议…", "Cargando reuniones…"));
    }

    void IniciarGravacao(ContextoApps& c) {
        auto& app = ContextoApps::App();
        auto& cartao = CartaoWatcher::Instancia();
        tela_ = Tela::Gravando;
        gravando_ = true;
        pausado_ = false;
        botao_ = 0;
        acumulado_ = 0;
        trecho_inicio_ = ContextoApps::Agora();
        segundos_sem_canal_ = 0;
        houve_queda_ = !RedeWatcher::Online();
        arquivo_ = cartao.IniciarReuniao();
        if (!arquivo_.empty()) {
            app.DefinirGanchoAudio([this](const std::vector<uint8_t>& p) {
                if (!pausado_) {
                    CartaoWatcher::Instancia().GravarPacote(p);
                }
            });
        }
        c.painel.MostrarGravacao(0, false, botao_);
        app.GravacaoLocal(true);  // microfone ligado mesmo sem conexão
        if (RedeWatcher::Online()) {
            app.StartListening();
            avisar_servidor_ = true;
        }
    }

    void Girar(ContextoApps& c, int passo) override {
        if (tela_ == Tela::Gravando) {
            botao_ = passo > 0 ? 1 : 0;
            c.painel.MostrarGravacao(Segundos(), pausado_, botao_);
        } else {
            c.painel.Mover(passo);
        }
    }

    bool Clicar(ContextoApps& c) override {
        if (tela_ == Tela::Gravando) {
            if (botao_ == 0) {
                AlternarPausa();
                c.painel.MostrarGravacao(Segundos(), pausado_, botao_);
            } else {
                Parar(c);
            }
            return true;
        }
        if (tela_ == Tela::Carregando) {
            return true;
        }
        int i = c.painel.Selecionado();
        if (tela_ == Tela::Lista && i == 1) {  // Gravar nova
            IniciarGravacao(c);
            return true;
        }
        if (tela_ == Tela::Lista && i > 1 && i <= (int)lista_.size() + 1) {
            // Detalhe: duração e resumo, buscados no Mac (no Tique, fora da trava da gaveta)
            atual_ = i - 2;
            tela_ = Tela::Carregando;
            buscar_detalhe_ = true;
            c.painel.MostrarStatus(lista_[atual_].titulo, PainelWatcher::Status::Carregando,
                                   TR("Abrindo a reunião…", "Opening meeting…", "正在打开会议…", "Abriendo la reunión…"));
            return true;
        }
        if (tela_ == Tela::Item) {
            MostrarLista(c);
            return true;
        }
        return false;  // Voltar
    }

    bool Voltar(ContextoApps& c) override {
        if (tela_ == Tela::Item) {
            MostrarLista(c);
            return true;
        }
        return false;
    }

    void Tique(ContextoApps& c) override {
        if (tela_ == Tela::Carregando && buscar_detalhe_.exchange(false)) {
            const auto& r = lista_[atual_];
            std::string corpo, texto;
            if (RedeWatcher::Pedir("GET", "/watcher/reunioes/" + Codificar(r.id), "", corpo)) {
                cJSON* raiz = cJSON_Parse(corpo.c_str());
                texto = RedeWatcher::Campo(raiz, "texto");
                cJSON_Delete(raiz);
            }
            tela_ = Tela::Item;
            c.painel.MostrarTexto(r.titulo, texto.empty() ? r.detalhe : texto, {TR("Voltar", "Back", "返回", "Volver")});
            return;
        }
        if (tela_ == Tela::Carregando) {
            if (ContextoApps::Agora() >= buscar_em_) {
                BuscarLista(c);
            }
            return;
        }
        if (!gravando_) {
            return;
        }
        auto& app = ContextoApps::App();
        if (avisar_servidor_ && app.GetDeviceState() == kDeviceStateListening) {
            avisar_servidor_ = false;
            app.EnviarJson("{\"type\":\"reuniao\",\"state\":\"start\"}");
        }
        if (app.GetDeviceState() != kDeviceStateListening) {
            if (++segundos_sem_canal_ > 12) {  // ~6 s sem canal de voz: segue só no cartão
                houve_queda_ = true;
            }
        } else {
            segundos_sem_canal_ = 0;
        }
        c.painel.MostrarGravacao(Segundos(), pausado_, botao_);
    }

private:
    enum class Tela { Gravando, Carregando, Lista, Item };
    std::atomic<bool> buscar_detalhe_{false};
    int atual_ = 0;

    static std::string Codificar(const std::string& texto) {  // id tem espaço ("2026-10-05 19h55")
        static const char* hex = "0123456789ABCDEF";
        std::string saida;
        for (unsigned char ch : texto) {
            if (isalnum(ch) || ch == '-' || ch == '_' || ch == '.') {
                saida += (char)ch;
            } else {
                saida += '%';
                saida += hex[ch >> 4];
                saida += hex[ch & 15];
            }
        }
        return saida;
    }
    struct Reuniao {
        std::string id, titulo, detalhe;
    };
    std::atomic<Tela> tela_{Tela::Gravando};
    std::atomic<bool> gravando_{false};
    std::atomic<bool> pausado_{false};
    std::atomic<bool> avisar_servidor_{false};
    int botao_ = 0;            // 0 = Pausar/Continuar, 1 = Parar
    int acumulado_ = 0;        // segundos gravados antes da pausa atual
    int trecho_inicio_ = 0;
    int segundos_sem_canal_ = 0;
    int buscar_em_ = 0;
    bool houve_queda_ = false;
    std::string arquivo_;
    std::vector<Reuniao> lista_;

    int Segundos() const { return acumulado_ + (pausado_ ? 0 : ContextoApps::Agora() - trecho_inicio_); }

    void AlternarPausa() {
        auto& app = ContextoApps::App();
        if (pausado_) {
            trecho_inicio_ = ContextoApps::Agora();
            pausado_ = false;
        } else {
            acumulado_ += ContextoApps::Agora() - trecho_inicio_;
            pausado_ = true;
        }
        if (app.GetDeviceState() == kDeviceStateListening) {
            app.EnviarJson(pausado_ ? "{\"type\":\"reuniao\",\"state\":\"pause\"}"
                                    : "{\"type\":\"reuniao\",\"state\":\"resume\"}");
        }
    }

    void Parar(ContextoApps& c) {
        auto& app = ContextoApps::App();
        int segundos = Segundos();
        if (app.GetDeviceState() == kDeviceStateListening) {
            app.EnviarJson("{\"type\":\"reuniao\",\"state\":\"stop\"}");
            app.StopListening();
        }
        app.GravacaoLocal(false);
        app.DefinirGanchoAudio(nullptr);
        CartaoWatcher::Instancia().PararReuniao();
        if (!arquivo_.empty() && !houve_queda_) {
            CartaoWatcher::Mover(arquivo_, "/enviados/");  // o Mac já tem a reunião inteira
        }
        gravando_ = false;
        pausado_ = false;
        if (houve_queda_) {
            int minutos = (segundos + 30) / 60;
            ContextoApps::Avisar(TR("Reunião salva", "Meeting saved", "会议已保存", "Reunión guardada"),
                                 std::to_string(minutos) +
                                     TR(" min no cartão. Envio ao Mac quando houver conexão.",
                                        " min on the card. Sending to the Mac once online.",
                                        " 分钟已存到卡上，联网后发送到 Mac。",
                                        " min en la tarjeta. Se enviará al Mac cuando haya conexión."),
                                 "happy");
        }
        // Lista das reuniões gravadas (espera o servidor registrar a que acabou de terminar)
        tela_ = Tela::Carregando;
        buscar_em_ = ContextoApps::Agora() + 2;
        c.painel.MostrarStatus(TR("Gravador", "Recorder", "录音机", "Grabadora"), PainelWatcher::Status::Carregando,
                               TR("Salvando a reunião…", "Saving meeting…", "正在保存会议…", "Guardando la reunión…"));
    }

    void BuscarLista(ContextoApps& c) {
        std::string corpo;
        if (!RedeWatcher::Pedir("GET", "/watcher/reunioes", "", corpo)) {
            lista_.clear();
            MostrarLista(c);  // sem conexão: dá para gravar mesmo assim (fica no microSD)
            return;
        }
        lista_.clear();
        cJSON* raiz = cJSON_Parse(corpo.c_str());
        cJSON* itens = raiz ? cJSON_GetObjectItem(raiz, "reunioes") : nullptr;
        cJSON* item = nullptr;
        cJSON_ArrayForEach(item, itens) {
            lista_.push_back({RedeWatcher::Campo(item, "id"), RedeWatcher::Campo(item, "titulo"),
                              RedeWatcher::Campo(item, "detalhe")});
        }
        cJSON_Delete(raiz);
        MostrarLista(c);
    }

    void MostrarLista(ContextoApps& c) {
        tela_ = Tela::Lista;
        std::vector<PainelWatcher::Item> itens = {
            {TR("Voltar", "Back", "返回", "Volver"), "", MATERIAL_SYMBOLS_ARROW_BACK},
            {TR("Gravar nova", "Record new", "新建录音", "Grabar nueva"),
             TR("Começa a gravar agora", "Start recording now", "立即开始录音", "Empieza a grabar ya"), MATERIAL_SYMBOLS_MIC}};
        for (const auto& r : lista_) {
            itens.push_back({r.titulo, r.detalhe, MATERIAL_SYMBOLS_SCHEDULE});
        }
        c.painel.MostrarLista(TR("Gravador", "Recorder", "录音机", "Grabadora"), itens, 1);
    }
};

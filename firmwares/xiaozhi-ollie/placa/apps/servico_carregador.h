// Serviço (sem tela na gaveta): ao encaixar o cabo de carregamento, o Clawd leva um choque
// (animação "choque" por ~3 s) e depois volta à expressão do estado atual.
#pragma once

#include "../idioma_watcher.h"
#include "../nucleo_apps.h"

class ServicoCarregador : public AppWatcher {
public:
    const char* Nome() const override { return TR("Carregador", "Charger", "充电器", "Cargador"); }
    bool Visivel() const override { return false; }
    void Abrir(ContextoApps& c) override {}
    bool Clicar(ContextoApps& c) override { return false; }

    void Fundo(ContextoApps& c) override {
        int agora = ContextoApps::Agora();
        if (fim_choque_ > 0 && agora >= fim_choque_) {
            fim_choque_ = 0;
            ContextoApps::App().Schedule([]() {
                auto& app = ContextoApps::App();
                auto estado = app.GetDeviceState();
                const char* emocao = estado == kDeviceStateListening ? "ouvindo"
                                     : estado == kDeviceStateSpeaking ? "falando"
                                                                      : "happy";  // carregando: contente
                Board::GetInstance().GetDisplay()->SetEmotion(emocao);
            });
        }
        if (agora - ultima_leitura_ < 1) {
            return;
        }
        ultima_leitura_ = agora;
        int nivel = 0;
        bool carregando = false, descarregando = false;
        if (!Board::GetInstance().GetBatteryLevel(nivel, carregando, descarregando)) {
            return;
        }
        if (carregando && !carregando_antes_ && iniciado_) {
            fim_choque_ = agora + 3;
            ContextoApps::App().Schedule([]() { Board::GetInstance().GetDisplay()->SetEmotion("choque"); });
        }
        carregando_antes_ = carregando;
        iniciado_ = true;  // a primeira leitura só registra o estado (ligar já no cabo não dá choque)
    }

private:
    int ultima_leitura_ = 0;
    int fim_choque_ = 0;
    bool carregando_antes_ = false;
    bool iniciado_ = false;
};

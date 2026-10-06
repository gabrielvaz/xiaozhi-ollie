// Serviço (sem tela na gaveta): se o Watcher começou a ouvir (clique, ativação ou depois de uma resposta)
// e não detectou voz em 8 s, encerra a escuta e volta para a espera. Usa o VAD do próprio aparelho.
// Se ouviu voz, não encerra mais nesse trecho: o servidor pode levar vários segundos pensando ou usando
// ferramentas antes de responder (encerrar aí cortava a resposta). Não age durante a gravação de reunião.
#pragma once

#include "../idioma_watcher.h"
#include "../nucleo_apps.h"

class ServicoSilencio : public AppWatcher {
public:
    const char* Nome() const override { return TR("Silêncio", "Silence", "静音", "Silencio"); }
    bool Visivel() const override { return false; }
    void Abrir(ContextoApps& c) override {}
    bool Clicar(ContextoApps& c) override { return false; }

    void Fundo(ContextoApps& c) override {
        auto& app = ContextoApps::App();
        if (app.GetDeviceState() != kDeviceStateListening || app.GravandoLocal()) {
            ouvindo_desde_ = -1;
            ouviu_voz_ = false;
            return;
        }
        int agora = ContextoApps::Agora();
        if (app.IsVoiceDetected()) {
            ouviu_voz_ = true;  // falou: agora é o servidor que responde, sem prazo
        }
        if (ouviu_voz_) {
            return;
        }
        if (ouvindo_desde_ < 0) {
            ouvindo_desde_ = agora;  // começou a ouvir agora
            return;
        }
        if (agora - ouvindo_desde_ >= kSilencioS) {
            ouvindo_desde_ = -1;
            ESP_LOGI("ServicoSilencio", "Nenhuma voz em %d s: encerrando a escuta", kSilencioS);
            app.Schedule([]() {
                auto& a = ContextoApps::App();
                if (a.GetDeviceState() == kDeviceStateListening && !a.IsVoiceDetected()) {
                    a.ToggleChatState();  // ouvindo -> fecha o canal e volta para a espera
                }
            });
        }
    }

private:
    static constexpr int kSilencioS = 8;
    int ouvindo_desde_ = -1;
    bool ouviu_voz_ = false;
};

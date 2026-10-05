// Serviço (sem tela na gaveta): se o Watcher começou a ouvir (clique, ativação ou depois de uma resposta)
// e não detectou voz em 8 s, encerra a escuta e volta para a espera. Usa o VAD do próprio aparelho.
// Não age durante a gravação de reunião.
#pragma once

#include "../nucleo_apps.h"

class ServicoSilencio : public AppWatcher {
public:
    const char* Nome() const override { return "Silêncio"; }
    bool Visivel() const override { return false; }
    void Abrir(ContextoApps& c) override {}
    bool Clicar(ContextoApps& c) override { return false; }

    void Fundo(ContextoApps& c) override {
        auto& app = ContextoApps::App();
        if (app.GetDeviceState() != kDeviceStateListening || app.GravandoLocal()) {
            ouvindo_desde_ = -1;
            return;
        }
        int agora = ContextoApps::Agora();
        if (ouvindo_desde_ < 0 || app.IsVoiceDetected()) {
            ouvindo_desde_ = agora;  // começou a ouvir agora, ou ouviu voz: recomeça a contar
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
};

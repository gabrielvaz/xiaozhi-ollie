// Serviço (sem tela na gaveta): anota mudanças de estado no rastro de diagnóstico, manda um pulso a cada 30 s
// ao Mac (memória livre, estado, tempo ligado) e, depois de um reinício anormal (pânico, watchdog, queda de
// energia), manda o motivo e o rastro do que acontecia antes. Ver diagnostico_watcher.h.
#pragma once

#include <esp_heap_caps.h>

#include "../diagnostico_watcher.h"
#include "../nucleo_apps.h"

class ServicoDiagnostico : public AppWatcher {
public:
    const char* Nome() const override { return "Diagnóstico"; }
    const char* Id() const override { return "diagnostico"; }
    bool Visivel() const override { return false; }
    void Abrir(ContextoApps& c) override {}
    bool Clicar(ContextoApps& c) override { return false; }

    void Fundo(ContextoApps& c) override {
        auto estado = ContextoApps::App().GetDeviceState();
        if (estado != estado_anterior_) {
            DiagnosticoWatcher::Marcar("estado %d -> %d", (int)estado_anterior_, (int)estado);
            estado_anterior_ = estado;
        }
        int agora = ContextoApps::Agora();
        auto& cartao = CartaoWatcher::Instancia();
        if (cartao.Montado()) {
            if (!leu_sessao_anterior_) {  // antes de gravar a sessão atual por cima
                leu_sessao_anterior_ = true;
                sessao_anterior_ = DiagnosticoWatcher::FimDaSessaoAnterior(CartaoWatcher::kPasta);
            }
            if (agora - ultima_gravacao_ >= 5) {
                ultima_gravacao_ = agora;
                DiagnosticoWatcher::GravarNoCartao(CartaoWatcher::kPasta);
            }
        }
        if (!RedeWatcher::Online()) {
            return;
        }
        if (!sessao_anterior_.empty()) {  // fim da sessão anterior (útil quando foi desligado na mão)
            std::string corpo = "{\"tipo\":\"sessao_anterior\",\"motivo_deste_inicio\":\"" +
                                std::string(DiagnosticoWatcher::NomeMotivo(DiagnosticoWatcher::Motivo())) +
                                "\",\"rastro\":\"" + Escapar(sessao_anterior_) + "\"}";
            std::string resposta;
            if (RedeWatcher::Pedir("POST", "/watcher/diagnostico", corpo, resposta)) {
                sessao_anterior_.clear();
            }
        }
        if (DiagnosticoWatcher::RelatorioPendente()) {
            std::string corpo = "{\"tipo\":\"reinicio\",\"motivo\":\"" +
                                std::string(DiagnosticoWatcher::NomeMotivo(DiagnosticoWatcher::Motivo())) +
                                "\",\"rastro\":\"" + Escapar(DiagnosticoWatcher::RastroAnterior()) + "\"}";
            std::string resposta;
            if (RedeWatcher::Pedir("POST", "/watcher/diagnostico", corpo, resposta)) {
                DiagnosticoWatcher::RelatorioEnviado();
            }
        }
        // Em conversa (conectando, ouvindo, falando) o pulso abriria uma 2ª conexão HTTPS junto com a da voz,
        // com ~40 KB de RAM interna livre: atrasava a resposta e é suspeita de travar o aparelho falando.
        // Fora da espera, só um pulso a cada 150 s (o servidor acusa "sem pulso" depois de 3 min)
        int intervalo = estado == kDeviceStateIdle ? 30 : 150;
        if (agora - ultimo_pulso_ < intervalo) {
            return;
        }
        ultimo_pulso_ = agora;
        char corpo[320];
        snprintf(corpo, sizeof(corpo),
                 "{\"tipo\":\"pulso\",\"ligado_s\":%d,\"estado\":%d,\"heap_k\":%u,\"heap_min_k\":%u,"
                 "\"psram_k\":%u,\"maior_bloco_k\":%u}",
                 agora, (int)estado, (unsigned)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024),
                 (unsigned)(heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL) / 1024),
                 (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024),
                 (unsigned)(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL) / 1024));
        std::string resposta;
        RedeWatcher::Pedir("POST", "/watcher/diagnostico", corpo, resposta);
        DiagnosticoWatcher::Marcar("pulso");
    }

private:
    DeviceState estado_anterior_ = kDeviceStateUnknown;
    int ultimo_pulso_ = -1000;
    int ultima_gravacao_ = 0;
    bool leu_sessao_anterior_ = false;
    std::string sessao_anterior_;

    static std::string Escapar(const std::string& texto) {
        std::string saida;
        for (char ch : texto) {
            if (ch == '"' || ch == '\\') {
                saida += '\\';
                saida += ch;
            } else if (ch == '\n') {
                saida += "\\n";
            } else if ((unsigned char)ch >= 0x20) {
                saida += ch;
            }
        }
        return saida;
    }
};

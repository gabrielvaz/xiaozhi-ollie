// Diagnóstico de travamentos do Watcher.
// - Rastro: os últimos eventos (estado, app aberto, memória livre) numa área da RAM que sobrevive a
//   reinícios por pânico ou watchdog (RTC_NOINIT). Ao religar, se o motivo foi anormal, o rastro vai ao Mac.
// - Pulso: a cada minuto o aparelho manda ao Mac memória livre, estado e app aberto. Se ele congelar sem
//   reiniciar, o buraco entre os pulsos aparece no registro do servidor (data/diagnostico.log).
#pragma once

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>

#include <esp_attr.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <esp_timer.h>

// Rastro na memória RTC (não é zerada em reinícios por pânico/watchdog). O placa/ é incluído num único
// arquivo (sensecap_watcher.cc), então a variável fica aqui mesmo, com ligação interna.
struct RastroDiagnostico {
    static constexpr int kEntradas = 24;
    static constexpr int kTamanho = 56;
    uint32_t magia;
    uint32_t inicios;
    uint32_t proxima;
    char entradas[kEntradas][kTamanho];
};
static RTC_NOINIT_ATTR RastroDiagnostico rastro_diagnostico_;

class DiagnosticoWatcher {
public:
    static constexpr int kEntradas = RastroDiagnostico::kEntradas;
    static constexpr int kTamanho = RastroDiagnostico::kTamanho;

    // Chamado uma vez ao ligar: guarda o motivo do reinício e o rastro anterior (se houver)
    static void Iniciar() {
        motivo_ = esp_reset_reason();
        if (rastro_.magia == kMagia && Anormal(motivo_)) {
            relatorio_pendente_ = true;
            for (int i = 0; i < kEntradas; i++) {
                const char* e = rastro_.entradas[(rastro_.proxima + i) % kEntradas];
                if (e[0] != '\0') {
                    rastro_anterior_ += std::string(rastro_anterior_.empty() ? "" : "\n") + e;
                }
            }
        }
        if (rastro_.magia != kMagia) {
            memset(&rastro_, 0, sizeof(rastro_));
            rastro_.magia = kMagia;
        }
        rastro_.inicios++;
        Marcar("liga (motivo %s, inicio %lu)", NomeMotivo(motivo_), (unsigned long)rastro_.inicios);
    }

    // Anota um evento no rastro: "  123s heap 61k/48k | texto"
    static void Marcar(const char* formato, ...) {
        char texto[kTamanho];
        va_list args;
        va_start(args, formato);
        vsnprintf(texto, sizeof(texto), formato, args);
        va_end(args);
        char linha[kTamanho + 24];
        snprintf(linha, sizeof(linha), "%5ds %3uk %s", (int)(esp_timer_get_time() / 1000000),
                 (unsigned)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024), texto);
        char* destino = rastro_.entradas[rastro_.proxima % kEntradas];
        strncpy(destino, linha, kTamanho - 1);  // corta o que passar do espaço da entrada
        destino[kTamanho - 1] = '\0';
        rastro_.proxima = (rastro_.proxima + 1) % kEntradas;
    }

    static bool RelatorioPendente() { return relatorio_pendente_; }
    static void RelatorioEnviado() { relatorio_pendente_ = false; }
    static const std::string& RastroAnterior() { return rastro_anterior_; }
    static esp_reset_reason_t Motivo() { return motivo_; }

    static bool Anormal(esp_reset_reason_t m) {
        return m == ESP_RST_PANIC || m == ESP_RST_INT_WDT || m == ESP_RST_TASK_WDT || m == ESP_RST_WDT ||
               m == ESP_RST_BROWNOUT;
    }

    static const char* NomeMotivo(esp_reset_reason_t m) {
        switch (m) {
            case ESP_RST_POWERON: return "ligado";
            case ESP_RST_SW: return "reinicio-pedido";
            case ESP_RST_PANIC: return "PANICO";
            case ESP_RST_INT_WDT: return "WATCHDOG-INTERRUPCAO";
            case ESP_RST_TASK_WDT: return "WATCHDOG-TAREFA";
            case ESP_RST_WDT: return "WATCHDOG";
            case ESP_RST_BROWNOUT: return "QUEDA-DE-ENERGIA";
            case ESP_RST_DEEPSLEEP: return "sono";
            case ESP_RST_EXT: return "pino-externo";
            case ESP_RST_USB: return "usb";
            default: return "outro";
        }
    }

private:
    static constexpr uint32_t kMagia = 0x4F4C4C31;  // "OLL1"
    static inline RastroDiagnostico& rastro_ = rastro_diagnostico_;
    inline static esp_reset_reason_t motivo_ = ESP_RST_UNKNOWN;
    inline static bool relatorio_pendente_ = false;
    inline static std::string rastro_anterior_;
};

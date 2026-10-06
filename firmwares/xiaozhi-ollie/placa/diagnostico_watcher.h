// Diagnóstico de travamentos do Watcher.
// - Rastro: os últimos eventos (estado, app aberto, memória livre) numa área da RAM que sobrevive a
//   reinícios por pânico ou watchdog (RTC_NOINIT). Ao religar, se o motivo foi anormal, o rastro vai ao Mac.
// - Cópia no microSD (/sdcard/watcher/diagnostico.log): sobrevive até a desligar e ligar na mão (que apaga a
//   RAM); ao ligar, o fim da sessão anterior vai ao Mac.
// - Pulso: a cada 30 s o aparelho manda ao Mac memória livre, estado e app aberto. Se ele congelar sem
//   reiniciar, o buraco entre os pulsos aparece no registro do servidor (data/diagnostico.log).
#pragma once

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>

#include <sys/stat.h>

#include <esp_attr.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <esp_lvgl_port.h>

// Rastro na memória RTC (não é zerada em reinícios por pânico/watchdog). O placa/ é incluído num único
// arquivo (sensecap_watcher.cc), então a variável fica aqui mesmo, com ligação interna.
struct RastroDiagnostico {
    static constexpr int kEntradas = 24;
    static constexpr int kTamanho = 56;
    uint32_t magia;
    uint32_t inicios;
    uint32_t proxima;
    uint32_t travou;  // 1 = o vigia reiniciou o aparelho (reinício por software com o rastro preservado)
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
        travou_ = rastro_.magia == kMagia && motivo_ == ESP_RST_SW && rastro_.travou == 1;
        if (rastro_.magia == kMagia && (Anormal(motivo_) || travou_)) {
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
        rastro_.travou = 0;
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
        std::lock_guard<std::mutex> trava(trava_);
        if (pendente_.size() < 8192) {
            pendente_ += std::string(linha) + "\n";
        }
    }

    // Grava no microSD o que foi anotado desde a última vez (chamado a cada poucos segundos)
    static void GravarNoCartao(const char* pasta) {
        std::string bloco;
        {
            std::lock_guard<std::mutex> trava(trava_);
            bloco.swap(pendente_);
        }
        if (bloco.empty()) {
            return;
        }
        std::string caminho = std::string(pasta) + "/diagnostico.log";
        struct stat info;
        if (stat(caminho.c_str(), &info) == 0 && info.st_size > 256 * 1024) {
            rename(caminho.c_str(), (caminho + ".antigo").c_str());  // limita o tamanho
        }
        if (FILE* f = fopen(caminho.c_str(), "a")) {
            fwrite(bloco.data(), 1, bloco.size(), f);
            fclose(f);
        }
    }

    // Últimas linhas da sessão anterior no microSD (até a linha "liga" desta sessão, que ainda não foi gravada)
    static std::string FimDaSessaoAnterior(const char* pasta, size_t bytes = 1600) {
        std::string caminho = std::string(pasta) + "/diagnostico.log";
        FILE* f = fopen(caminho.c_str(), "r");
        if (f == nullptr) {
            return "";
        }
        fseek(f, 0, SEEK_END);
        long tamanho = ftell(f);
        long inicio = tamanho > (long)bytes ? tamanho - (long)bytes : 0;
        fseek(f, inicio, SEEK_SET);
        std::string texto(tamanho - inicio, '\0');
        size_t lidos = fread(texto.data(), 1, texto.size(), f);
        fclose(f);
        texto.resize(lidos);
        auto quebra = texto.find('\n');
        return inicio > 0 && quebra != std::string::npos ? texto.substr(quebra + 1) : texto;
    }

    // Vigia: a cada 5 s confere se a tela (LVGL) e a tarefa dos apps ainda respondem. Se a tela ficar presa
    // 30 s ou a tarefa dos apps 120 s (pedidos de rede longos chegam a 90 s), anota o que estava rodando e
    // reinicia: o rastro sobrevive ao reinício por software e vai ao Mac no próximo início.
    static void Batida(const char* fase) {
        fase_ = fase;
        ultima_batida_ = esp_timer_get_time();
    }
    static void IniciarVigia() {
        ultima_batida_ = esp_timer_get_time();
        esp_timer_create_args_t args = {};
        args.callback = [](void*) { Vigiar(); };
        args.name = "vigia_watcher";
        esp_timer_handle_t timer;
        if (esp_timer_create(&args, &timer) == ESP_OK) {
            esp_timer_start_periodic(timer, 5 * 1000000);
        }
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
        if (m == ESP_RST_SW && travou_) {
            return "TRAVOU-REINICIADO-PELO-VIGIA";
        }
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
    static constexpr uint32_t kMagia = 0x4F4C4C32;  // "OLL2" (campo travou novo: invalida o rastro antigo)

    static void Vigiar() {
        bool tela_ok = lvgl_port_lock(100);
        if (tela_ok) {
            lvgl_port_unlock();
            tela_presa_ = 0;
        } else {
            tela_presa_++;
        }
        int64_t sem_batida_s = (esp_timer_get_time() - ultima_batida_) / 1000000;
        if (tela_presa_ >= 6 || sem_batida_s >= 120) {
            Marcar("TRAVOU: %s (tela presa %ds, apps parados %ds)", fase_ ? fase_ : "?", tela_presa_ * 5,
                   (int)sem_batida_s);
            rastro_.travou = 1;
            esp_restart();
        }
    }

    inline static volatile int64_t ultima_batida_ = 0;
    inline static const char* volatile fase_ = nullptr;
    inline static int tela_presa_ = 0;
    inline static bool travou_ = false;
    static inline RastroDiagnostico& rastro_ = rastro_diagnostico_;
    inline static esp_reset_reason_t motivo_ = ESP_RST_UNKNOWN;
    inline static bool relatorio_pendente_ = false;
    inline static std::string rastro_anterior_;
    inline static std::mutex trava_;
    inline static std::string pendente_;  // linhas ainda não gravadas no microSD
};

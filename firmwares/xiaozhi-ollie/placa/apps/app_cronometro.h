// App "Cronômetro": a roda escolhe Iniciar/Pausar, Zerar ou Voltar; continua correndo com a gaveta fechada.
#pragma once

#include "../idioma_watcher.h"
#include "../nucleo_apps.h"

class AppCronometro : public AppWatcher {
public:
    const char* Nome() const override { return TR("Cronômetro", "Stopwatch", "秒表", "Cronómetro"); }
    const char* Icone() const override { return PainelWatcher::kIconeAmpulheta; }  // desenhada
    std::string Detalhe() const override {
        return correndo_ ? TR("Correndo: ", "Running: ", "计时中：", "En marcha: ") + Formatar(Decorrido())
                         : TR("Iniciar, pausar e zerar", "Start, pause, reset", "开始、暂停、清零",
                              "Iniciar, pausar, reiniciar");
    }

    void Abrir(ContextoApps& c) override { Desenhar(c, 0); }

    void Girar(ContextoApps& c, int passo) override { c.painel.Mover(passo); }

    bool Clicar(ContextoApps& c) override {
        int i = c.painel.Selecionado();
        if (i == 0) {  // Iniciar / Pausar
            if (correndo_) {
                acumulado_ms_ = Decorrido();
                correndo_ = false;
            } else {
                inicio_ms_ = Agora();
                correndo_ = true;
            }
            Desenhar(c, 0);
            return true;
        }
        if (i == 1) {  // Zerar
            correndo_ = false;
            acumulado_ms_ = 0;
            Desenhar(c, 1);
            return true;
        }
        return false;  // Voltar
    }


private:
    bool correndo_ = false;
    int64_t inicio_ms_ = 0;
    int64_t acumulado_ms_ = 0;

    static int64_t Agora() { return esp_timer_get_time() / 1000; }
    int64_t Decorrido() const { return acumulado_ms_ + (correndo_ ? Agora() - inicio_ms_ : 0); }

    // mm:ss.mmm (ou h:mm:ss.mmm)
    static std::string Formatar(int64_t ms) {
        int s = ms / 1000;
        int mili = ms % 1000;
        char t[32];
        if (s >= 3600) {
            snprintf(t, sizeof(t), "%d:%02d:%02d.%03d", s / 3600, s / 60 % 60, s % 60, mili);
        } else {
            snprintf(t, sizeof(t), "%02d:%02d.%03d", s / 60, s % 60, mili);
        }
        return t;
    }

    void Desenhar(ContextoApps& c, int selecionado) {
        c.painel.MostrarValor(TR("Cronômetro", "Stopwatch", "秒表", "Cronómetro"), Formatar(Decorrido()),
                              correndo_ ? TR("correndo", "running", "计时中", "en marcha")
                                        : TR("parado", "stopped", "已停止", "parado"),
                              {correndo_ ? TR("Pausar", "Pause", "暂停", "Pausar") : TR("Iniciar", "Start", "开始", "Iniciar"),
                               TR("Zerar", "Reset", "清零", "Reiniciar"), TR("Voltar", "Back", "返回", "Volver")},
                              selecionado);
        if (correndo_) {  // milissegundos: atualiza a tela a cada ~33 ms
            c.painel.AtualizarRapido([this]() { return Formatar(Decorrido()); }, 33);
        }
    }
};

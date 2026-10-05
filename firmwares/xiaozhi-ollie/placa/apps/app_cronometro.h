// App "Cronômetro": a roda escolhe Iniciar/Pausar, Zerar ou Voltar; continua correndo com a gaveta fechada.
#pragma once

#include "../nucleo_apps.h"

class AppCronometro : public AppWatcher {
public:
    const char* Nome() const override { return "Cronômetro"; }
    const char* Icone() const override { return MATERIAL_SYMBOLS_WATCH; }
    std::string Detalhe() const override { return correndo_ ? "Correndo: " + Formatar(Decorrido()) : "Iniciar, pausar e zerar"; }

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
        c.painel.MostrarValor("Cronômetro", Formatar(Decorrido()), correndo_ ? "correndo" : "parado",
                              {correndo_ ? "Pausar" : "Iniciar", "Zerar", "Voltar"}, selecionado);
        if (correndo_) {  // milissegundos: atualiza a tela a cada ~33 ms
            c.painel.AtualizarRapido([this]() { return Formatar(Decorrido()); }, 33);
        }
    }
};

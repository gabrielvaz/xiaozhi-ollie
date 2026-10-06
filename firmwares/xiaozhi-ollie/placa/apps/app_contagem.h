// App "Contagem regressiva": cada passo da roda soma/tira 30 s, o clique inicia; ao terminar, avisa na tela com som
// (mesmo com a gaveta fechada). Lembra o último tempo usado.
#pragma once

#include <algorithm>

#include "assets/lang_config.h"

#include "../idioma_watcher.h"
#include "../nucleo_apps.h"

class AppContagem : public AppWatcher {
public:
    const char* Nome() const override { return TR("Timer", "Timer", "倒计时", "Temporizador"); }
    const char* Id() const override { return "timer"; }
    const char* Icone() const override { return MATERIAL_SYMBOLS_ALARM; }
    std::string Detalhe() const override {
        return fim_s_ > 0 ? TR("Faltam ", "", "剩余 ", "Quedan ") + Formatar(fim_s_ - ContextoApps::Agora()) +
                                TR("", " left", "", "")
                          : TR("Timer com alarme", "Timer with alarm", "带闹铃的倒计时", "Temporizador con alarma");
    }

    void Abrir(ContextoApps& c) override {
        segundos_ = ConfigWatcher::Int("contagem_s", 300);
        Desenhar(c);
    }

    void Girar(ContextoApps& c, int passo) override {
        if (fim_s_ > 0) {
            c.painel.Mover(passo);  // escolhe Cancelar / Voltar
            return;
        }
        segundos_ = std::clamp(segundos_ + passo * 30, 30, 3 * 3600);  // 30 s por passo
        c.painel.AtualizarValor(Formatar(segundos_));
    }

    bool Clicar(ContextoApps& c) override {
        if (fim_s_ > 0) {
            if (c.painel.Selecionado() == 0) {  // Cancelar
                fim_s_ = 0;
                Desenhar(c);
                return true;
            }
            return false;
        }
        ConfigWatcher::SetInt("contagem_s", segundos_);
        total_s_ = segundos_;
        fim_s_ = ContextoApps::Agora() + segundos_;
        Desenhar(c);
        return true;
    }

    void Tique(ContextoApps& c) override {
        if (fim_s_ > 0) {
            c.painel.AtualizarValor(Formatar(std::max(0, fim_s_ - ContextoApps::Agora())));
        }
    }

    void Fundo(ContextoApps& c) override {
        if (fim_s_ > 0 && ContextoApps::Agora() >= fim_s_) {
            fim_s_ = 0;
            ContextoApps::Avisar(TR("Tempo esgotado", "Time's up", "时间到", "Tiempo agotado"),
                                 TR("A contagem de ", "The ", "", "La cuenta atrás de ") + Formatar(total_s_) +
                                     TR(" terminou", " timer is done", " 倒计时已结束", " ha terminado"),
                                 "shocked", Lang::Sounds::OGG_VIBRATION);
        }
    }

private:
    int segundos_ = 300;
    int total_s_ = 0;
    int fim_s_ = 0;  // 0 = parada

    static std::string Formatar(int s) {
        char t[24];
        if (s >= 3600) {
            snprintf(t, sizeof(t), "%d:%02d:%02d", s / 3600, s / 60 % 60, s % 60);
        } else {
            snprintf(t, sizeof(t), "%02d:%02d", s / 60, s % 60);
        }
        return t;
    }

    void Desenhar(ContextoApps& c) {
        if (fim_s_ > 0) {
            c.painel.MostrarValor(TR("Contagem regressiva", "Countdown", "倒计时", "Cuenta atrás"),
                                  Formatar(fim_s_ - ContextoApps::Agora()),
                                  TR("toca um alarme no fim", "alarm rings at the end", "结束时响铃",
                                     "suena una alarma al final"),
                                  {TR("Cancelar", "Cancel", "取消", "Cancelar"), TR("Voltar", "Back", "返回", "Volver")});
        } else {
            c.painel.MostrarValor(TR("Contagem regressiva", "Countdown", "倒计时", "Cuenta atrás"), Formatar(segundos_),
                                  TR("gire ±30 s · clique para iniciar", "turn ±30 s · click to start",
                                     "旋转 ±30 秒 · 按下开始", "gira ±30 s · pulsa para iniciar"),
                                  {});
        }
    }
};

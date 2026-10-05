// Fontes do texto: Noto Sans (padrão) ou JetBrains Mono (estilo terminal), escolhida em Configurações.
// Os ícones (Material Symbols) e a hora da tela inicial (fonte do tema) não mudam.
#pragma once

#include <lvgl.h>

#include "settings.h"

LV_FONT_DECLARE(font_noto_sans_pt_24);       // fonte/font_noto_sans_pt_24.c
LV_FONT_DECLARE(font_noto_sans_basic_20_4);  // do XiaoZhi
LV_FONT_DECLARE(font_jetbrains_mono_pt_22);  // fonte/font_jetbrains_mono_pt_22.c
LV_FONT_DECLARE(font_jetbrains_mono_pt_18);  // fonte/font_jetbrains_mono_pt_18.c

class Fontes {
public:
    static bool Mono() {
        int& estado = Estado();
        if (estado < 0) {  // lê a escolha uma vez só; depois fica em memória
            Settings s("watcher", false);
            estado = s.GetString("fonte") == "mono" ? 1 : 0;
        }
        return estado == 1;
    }

    static void DefinirMono(bool mono) {
        Settings s("watcher", true);
        s.SetString("fonte", mono ? "mono" : "noto");
        Estado() = mono ? 1 : 0;
    }

    // A Mono é mais larga: usa 2 px a menos para caber o mesmo texto por linha
    static const lv_font_t* Grande() { return Mono() ? &font_jetbrains_mono_pt_22 : &font_noto_sans_pt_24; }
    static const lv_font_t* Pequena() { return Mono() ? &font_jetbrains_mono_pt_18 : &font_noto_sans_basic_20_4; }
    static const char* Nome() { return Mono() ? "JetBrains Mono" : "Noto Sans"; }

private:
    static int& Estado() {
        static int estado = -1;
        return estado;
    }
};

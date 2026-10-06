// Fontes do texto: Noto Sans (padrão) ou JetBrains Mono (estilo terminal), escolhida em Configurações.
// Os ícones (Material Symbols) e a hora da tela inicial (fonte do tema) não mudam.
// Em chinês as fontes próprias não servem (não têm ideogramas): o texto usa a fonte Noto CJK do tema,
// carregada da partição de assets, e a opção Mono fica desligada.
#pragma once

#include <lvgl.h>

#include "idioma_watcher.h"
#include "lvgl_theme.h"
#include "settings.h"

LV_FONT_DECLARE(font_noto_sans_pt_24);       // fonte/font_noto_sans_pt_24.c
LV_FONT_DECLARE(font_noto_sans_basic_20_4);
LV_FONT_DECLARE(font_material_symbols_16_4);  // ícone do Wi-Fi na barra de status (menor)  // do XiaoZhi
LV_FONT_DECLARE(font_jetbrains_mono_pt_22);  // fonte/font_jetbrains_mono_pt_22.c
LV_FONT_DECLARE(font_jetbrains_mono_pt_18);  // fonte/font_jetbrains_mono_pt_18.c

class Fontes {
public:
    // A Mono só existe para alfabetos latinos
    static bool MonoDisponivel() {
#ifdef OLLIE_IDIOMA_CJK
        return false;
#else
        return true;
#endif
    }

    static bool Mono() {
        if (!MonoDisponivel()) {
            return false;
        }
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
    static const lv_font_t* Grande() {
#ifdef OLLIE_IDIOMA_CJK
        return FonteTema();
#else
        return Mono() ? &font_jetbrains_mono_pt_22 : &font_noto_sans_pt_24;
#endif
    }
    static const lv_font_t* Pequena() {
#ifdef OLLIE_IDIOMA_CJK
        return FonteTema();
#else
        return Mono() ? &font_jetbrains_mono_pt_18 : &font_noto_sans_basic_20_4;
#endif
    }
    static const char* Nome() { return Mono() ? "JetBrains Mono" : "Noto Sans"; }

private:
    static int& Estado() {
        static int estado = -1;
        return estado;
    }

#ifdef OLLIE_IDIOMA_CJK
    // Fonte de texto do tema (Noto Sans CJK da partição de assets); antes de os assets carregarem, a básica
    static const lv_font_t* FonteTema() {
        auto* tema = LvglThemeManager::GetInstance().GetTheme("dark");
        if (tema != nullptr && tema->text_font() != nullptr && tema->text_font()->font() != nullptr) {
            return tema->text_font()->font();
        }
        return &font_noto_sans_basic_20_4;
    }
#endif
};

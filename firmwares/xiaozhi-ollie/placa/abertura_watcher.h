// Tela de abertura: o Clawd acenando, o logo "Ollie", a frase e a versão.
// Fica na camada do sistema (acima da gaveta e dos avisos) e some sozinha depois de alguns segundos.
#pragma once

#include <lvgl.h>

#include <string>

#include "fontes_watcher.h"
#include "idioma_watcher.h"

LV_FONT_DECLARE(font_ollie_logo_88);  // fonte/font_ollie_logo_88.c: Inter Black, só as letras de "Ollie"

// Versão mostrada na abertura e em Configurações > Sobre: a mesma do OTA (PROJECT_VER, que vem de
// OLLIE_VERSAO_APP em aplicar_patches.py). No simulador não há descrição do app.
#if __has_include(<esp_app_desc.h>)
#include <esp_app_desc.h>
inline const char* OllieVersao() { return esp_app_get_description()->version; }
#else
inline const char* OllieVersao() { return "dev"; }
#endif
#define OLLIE_BASE_XIAOZHI "2.5.0"  // versão do XiaoZhi baixada por compilar.sh

class AberturaWatcher {
public:
    static void Mostrar() {
        auto tela = lv_obj_create(lv_layer_sys());
        lv_obj_remove_style_all(tela);
        lv_obj_set_size(tela, LV_PCT(100), LV_PCT(100));
        lv_obj_set_style_bg_color(tela, lv_color_hex(0x000000), 0);
        lv_obj_set_style_bg_opa(tela, LV_OPA_COVER, 0);
        lv_obj_remove_flag(tela, LV_OBJ_FLAG_SCROLLABLE);

        // Clawd em pixel art (mesma grade 20x18 do mascote-clawd/gerar.py, pose "waving")
        auto clawd = Caixa(tela, kGradeW * kPx, kGradeH * kPx);
        lv_obj_align(clawd, LV_ALIGN_TOP_MID, 0, 58);
        Pixel(clawd, 3, 4, 14, 9, kLaranja);               // corpo
        Pixel(clawd, 1, 8, 2, 2, kLaranja);                // braço esquerdo
        for (int x : {4, 6, 13, 15}) {
            Pixel(clawd, x, 13, 1, 2, kLaranja);           // perninhas
        }
        for (int x : {6, 13}) {                            // olhos felizes: ^ ^
            Pixel(clawd, x - 1, 7, 1, 1, kOlho);
            Pixel(clawd, x, 6, 1, 1, kOlho);
            Pixel(clawd, x + 1, 7, 1, 1, kOlho);
        }
        Pixel(clawd, 8, 10, 1, 1, kOlho);                  // sorriso
        Pixel(clawd, 9, 11, 2, 1, kOlho);
        Pixel(clawd, 11, 10, 1, 1, kOlho);
        Pixel(clawd, 4, 9, 2, 1, kRosa);                   // bochechas
        Pixel(clawd, 14, 9, 2, 1, kRosa);

        // Braço direito acenando: sobe e desce em dois tempos, como no GIF
        auto braco = Caixa(clawd, 3 * kPx, 3 * kPx);
        lv_obj_set_pos(braco, 17 * kPx, 5 * kPx);
        Pixel(braco, 0, 1, 2, 2, kLaranja);
        Pixel(braco, 2, 0, 1, 1, kLaranja);
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, braco);
        lv_anim_set_exec_cb(&a, [](void* obj, int32_t y) { lv_obj_set_y(static_cast<lv_obj_t*>(obj), y); });
        lv_anim_set_values(&a, 4 * kPx, 5 * kPx);
        lv_anim_set_duration(&a, 350);
        lv_anim_set_reverse_duration(&a, 350);
        lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
        lv_anim_set_path_cb(&a, lv_anim_path_step);
        lv_anim_start(&a);  // apagada junto com o objeto

        auto logo = Texto(tela, &font_ollie_logo_88, kLaranja, "Ollie");
        lv_obj_align(logo, LV_ALIGN_TOP_MID, 0, 196);
        auto frase = Texto(tela, Fontes::Pequena(), 0xC8C8C8, TR("de olho nos seus agentes", "keeps an eye on your agents",
                                                                    "keeps an eye on your agents", "vigila a tus agentes"));
        lv_obj_align(frase, LV_ALIGN_TOP_MID, 0, 284);
        auto versao = Texto(tela, Fontes::Pequena(), 0x6E6E6E, (std::string("v") + OllieVersao()).c_str());
        lv_obj_align(versao, LV_ALIGN_TOP_MID, 0, 316);

        lv_obj_fade_out(tela, 500, kDuracaoMs);
        lv_obj_delete_delayed(tela, kDuracaoMs + 550);
    }

private:
    static constexpr int kPx = 7;  // cada "pixel" do desenho
    static constexpr int kGradeW = 20;
    static constexpr int kGradeH = 18;
    static constexpr uint32_t kDuracaoMs = 3200;
    static constexpr uint32_t kLaranja = 0xD97757;
    static constexpr uint32_t kOlho = 0x1E1816;
    static constexpr uint32_t kRosa = 0xF08C96;

    static lv_obj_t* Caixa(lv_obj_t* pai, int w, int h) {
        auto obj = lv_obj_create(pai);
        lv_obj_remove_style_all(obj);
        lv_obj_set_size(obj, w, h);
        lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
        return obj;
    }

    static void Pixel(lv_obj_t* pai, int x, int y, int w, int h, uint32_t cor) {
        auto p = Caixa(pai, w * kPx, h * kPx);
        lv_obj_set_pos(p, x * kPx, y * kPx);
        lv_obj_set_style_bg_color(p, lv_color_hex(cor), 0);
        lv_obj_set_style_bg_opa(p, LV_OPA_COVER, 0);
    }

    static lv_obj_t* Texto(lv_obj_t* pai, const lv_font_t* fonte, uint32_t cor, const char* texto) {
        auto r = lv_label_create(pai);
        lv_obj_set_style_text_font(r, fonte, 0);
        lv_obj_set_style_text_color(r, lv_color_hex(cor), 0);
        lv_label_set_text(r, texto);
        return r;
    }
};

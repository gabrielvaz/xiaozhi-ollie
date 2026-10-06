// Tela do modo de configuração de Wi-Fi (sem rede salva ou a rede não conectou): título "Sem Wi-Fi" no
// topo, o Clawd confuso ("sem_wifi") com o corpo no centro e, embaixo, a rede do Ollie para entrar pelo
// celular e o endereço do portal. Fica na camada de cima até a configuração terminar (o aparelho reinicia).
// Quem chama trava a tela.
#pragma once

#include <lvgl.h>

#include <cstring>
#include <string>

#include "clawd_animado.h"
#include "fontes_watcher.h"
#include "idioma_watcher.h"
#include "layout_mascote.h"

class TelaSemWifi {
public:
    explicit TelaSemWifi(Display* display) : clawd_(display) {}

    void Mostrar(const std::string& rede, const std::string& url) {
        Esconder();
        tela_ = lv_obj_create(lv_layer_top());
        lv_obj_remove_style_all(tela_);
        lv_obj_set_size(tela_, LV_PCT(100), LV_PCT(100));
        lv_obj_set_style_bg_color(tela_, lv_color_hex(0x000000), 0);
        lv_obj_set_style_bg_opa(tela_, LV_OPA_COVER, 0);
        lv_obj_remove_flag(tela_, LV_OBJ_FLAG_SCROLLABLE);

        Linha(TR("Sem Wi-Fi", "No Wi-Fi", "未连接 Wi-Fi", "Sin Wi-Fi"), Fontes::Grande(), kLaranja, 58);

        // Corpo do Clawd no centro da tela, como nas outras telas (placa/layout_mascote.h)
        auto pos = LayoutMascote::Calcular(LayoutMascote::kEscalaCheia);
        if (auto img = clawd_.Criar(tela_, "sem_wifi")) {
            lv_obj_align(img, LV_ALIGN_CENTER, 0, pos.mascote_dy);
        }

        // Endereço sem o "http://" (o celular abre do mesmo jeito e cabe numa linha)
        std::string endereco = url;
        for (const char* prefixo : {"http://", "https://"}) {
            if (endereco.rfind(prefixo, 0) == 0) {
                endereco = endereco.substr(strlen(prefixo));
            }
        }
        if (!endereco.empty() && endereco.back() == '/') {
            endereco.pop_back();
        }
        int y = pos.texto_y;
        y += Linha(TR("No celular, entre na rede", "On your phone, join", "用手机连接网络", "En el móvil, conéctate a"),
                   Fontes::Pequena(), kCinza, y);
        y += Linha(rede, Fontes::Grande(), kBranco, y) + 4;
        y += Linha(TR("e abra no navegador", "then open in the browser", "然后在浏览器打开", "y abre en el navegador"),
                   Fontes::Pequena(), kCinza, y);
        Linha(endereco, Fontes::Grande(), kLaranja, y);
    }

    void Esconder() {
        clawd_.Parar();
        if (tela_ != nullptr) {
            lv_obj_delete(tela_);
            tela_ = nullptr;
        }
    }

private:
    static constexpr uint32_t kLaranja = 0xD97757;
    static constexpr uint32_t kBranco = 0xFFFFFF;
    static constexpr uint32_t kCinza = 0xB4B4B4;

    ClawdAnimado clawd_;
    lv_obj_t* tela_ = nullptr;

    // Uma linha centrada, sem quebrar (reticências se não couber); retorna a altura usada
    int Linha(const std::string& texto, const lv_font_t* fonte, uint32_t cor, int y) {
        auto r = lv_label_create(tela_);
        lv_obj_set_style_text_font(r, fonte, 0);
        lv_obj_set_style_text_color(r, lv_color_hex(cor), 0);
        lv_obj_set_style_text_align(r, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_width(r, 300);
        lv_label_set_long_mode(r, LV_LABEL_LONG_DOT);
        lv_label_set_text(r, texto.c_str());
        lv_obj_align(r, LV_ALIGN_TOP_MID, 0, y);
        return lv_font_get_line_height(fonte);
    }
};

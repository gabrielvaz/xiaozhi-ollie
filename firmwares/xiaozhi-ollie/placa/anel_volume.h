// Anel do volume: linha branca na borda da tela redonda, começando no topo e andando no sentido horário;
// o comprimento é o volume (100% fecha o círculo). Aparece enquanto a roda gira, por cima da tela principal.
// Quem chama trava a tela.
#pragma once

#include <lvgl.h>

#include <algorithm>

class AnelVolume {
public:
    void Mostrar(int volume) {
        if (arco_ == nullptr) {
            arco_ = lv_arc_create(lv_layer_top());
            lv_obj_set_size(arco_, kDiametro, kDiametro);
            lv_obj_center(arco_);
            lv_arc_set_rotation(arco_, 270);  // 0% no topo
            lv_arc_set_bg_angles(arco_, 0, 360);
            lv_arc_set_range(arco_, 0, 100);
            lv_obj_remove_style(arco_, nullptr, LV_PART_KNOB);
            lv_obj_remove_flag(arco_, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_pad_all(arco_, 0, 0);
            lv_obj_set_style_arc_width(arco_, kEspessura, LV_PART_MAIN);
            lv_obj_set_style_arc_width(arco_, kEspessura, LV_PART_INDICATOR);
            lv_obj_set_style_arc_color(arco_, lv_color_hex(0x2A2A2A), LV_PART_MAIN);  // trilho apagado
            lv_obj_set_style_arc_color(arco_, lv_color_hex(0xFFFFFF), LV_PART_INDICATOR);
        }
        lv_arc_set_value(arco_, std::clamp(volume, 0, 100));
        lv_obj_remove_flag(arco_, LV_OBJ_FLAG_HIDDEN);
    }

    void Esconder() {
        if (arco_ != nullptr) {
            lv_obj_add_flag(arco_, LV_OBJ_FLAG_HIDDEN);
        }
    }

private:
    static constexpr int kDiametro = 404;  // 4 px da borda: o vidro redondo corta um pouco a beirada
    static constexpr int kEspessura = 4;  // fina, por fora do Wi-Fi e da bateria (que descem 14 px)
    lv_obj_t* arco_ = nullptr;
};

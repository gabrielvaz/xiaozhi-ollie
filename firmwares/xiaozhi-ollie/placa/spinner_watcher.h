// Spinner do Ollie: arco laranja girando sobre um trilho escuro. O mesmo das telas das Configurações e
// das listas (placa/painel_watcher.h) e da tela de atualização do sistema. Quem chama trava a tela.
#pragma once

#include <lvgl.h>

inline lv_obj_t* SpinnerWatcher(lv_obj_t* pai, int tamanho, int espessura, uint32_t cor_fundo,
                                uint32_t cor = 0xD97757) {
    auto arco = lv_arc_create(pai);
    lv_obj_set_size(arco, tamanho, tamanho);
    lv_arc_set_bg_angles(arco, 0, 360);
    lv_arc_set_angles(arco, 0, 90);
    lv_obj_remove_style(arco, nullptr, LV_PART_KNOB);
    lv_obj_remove_flag(arco, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_pad_all(arco, 0, 0);
    lv_obj_set_style_arc_width(arco, espessura, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arco, espessura, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arco, lv_color_hex(cor_fundo), LV_PART_MAIN);
    lv_obj_set_style_arc_color(arco, lv_color_hex(cor), LV_PART_INDICATOR);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, arco);
    lv_anim_set_values(&a, 0, 360);
    lv_anim_set_duration(&a, 1000);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_exec_cb(&a, [](void* obj, int32_t v) { lv_arc_set_rotation((lv_obj_t*)obj, v); });
    lv_anim_start(&a);  // apagada junto com o objeto
    return arco;
}

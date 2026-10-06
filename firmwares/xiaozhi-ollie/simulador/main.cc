// Simulador da tela do Watcher (412x412, RGB565) no Mac: mesma versão do LVGL, mesmo decodificador de GIF
// e o mesmo layout do firmware. Cada cena vira um PNG em saida/ e o script medir.py mede o Clawd.
// Uso: ./rodar.sh   (compila, roda as cenas e mede)
#include <lvgl.h>

#include <dirent.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "gif/lvgl_gif.h"
#include "material_symbols.h"
#include "layout_mascote.h"  // a mesma regra do firmware
#include <algorithm>

LV_FONT_DECLARE(font_noto_sans_basic_30_4);   // BUILTIN_TEXT_FONT do Watcher
LV_FONT_DECLARE(font_material_symbols_20_4);  // BUILTIN_ICON_FONT do Watcher
LV_FONT_DECLARE(font_material_symbols_30_4);  // large_icon_font
LV_FONT_DECLARE(font_material_symbols_16_4);

#include "abertura_watcher.h"  // tela de abertura (placa/)
#include "fontes_watcher.h"    // Noto Sans ou JetBrains Mono (OLLIE_FONTE=mono)
#include "painel_watcher.h"    // telas dos apps (placa/), com o Clawd no carregamento
#include "anel_volume.h"       // anel do volume na borda
#include "tela_sem_wifi.h"     // modo de configuração de Wi-Fi

static constexpr int kLado = 412;
static uint16_t g_fb[kLado * kLado];
static uint32_t g_ms = 0;
static lv_obj_t* g_caixa_texto = nullptr;  // bottom_bar_
static bool g_painel_aberto = false;
// OLLIE_MEDIR=1: toda pose vira o Clawd parado e sem acessórios ("staticstate") e as cenas vão para
// saida/medida/; o medir.py mede o layout nelas (a posição não depende da pose)
static const bool g_medir = std::getenv("OLLIE_MEDIR") != nullptr;
static std::string Gif(const std::string& pose) {
    return "../mascote-clawd/emocoes/" + (g_medir ? std::string("staticstate") : pose) + ".gif";
}
static std::string Saida() { return g_medir ? "saida/medida/" : "saida/"; }       // painel dos apps por cima: a caixa da tela principal não conta

static void Flush(lv_display_t* disp, const lv_area_t*, uint8_t*) { lv_display_flush_ready(disp); }

// Avança o relógio simulado, rodando timers e animações (GIF, rolagem do status)
static void Avancar(uint32_t ms) {
    for (uint32_t t = 0; t < ms; t += 5) {
        g_ms += 5;
        lv_timer_handler();
    }
    lv_refr_now(nullptr);
}

static void SalvarPng(const std::string& nome) {
    lv_refr_now(nullptr);
    // PPM (RGB888); o medir.py converte para PNG
    std::string caminho = Saida() + nome + ".ppm";
    FILE* f = fopen(caminho.c_str(), "wb");
    fprintf(f, "P6\n%d %d\n255\n", kLado, kLado);
    for (int i = 0; i < kLado * kLado; i++) {
        uint16_t c = g_fb[i];
        uint8_t rgb[3] = {(uint8_t)(((c >> 11) & 0x1F) * 255 / 31), (uint8_t)(((c >> 5) & 0x3F) * 255 / 63),
                          (uint8_t)((c & 0x1F) * 255 / 31)};
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
    // Caixa do texto (se visível): o medir.py usa a caixa reservada, não só os pixels do texto
    FILE* g = fopen((Saida() + nome + ".txt").c_str(), "w");
    if (!g_painel_aberto && g_caixa_texto != nullptr && !lv_obj_has_flag(g_caixa_texto, LV_OBJ_FLAG_HIDDEN)) {
        lv_area_t a;
        lv_obj_get_coords(g_caixa_texto, &a);
        fprintf(g, "%d %d %d %d\n", (int)a.x1, (int)a.x2, (int)a.y1, (int)a.y2);
    }
    fclose(g);
    printf("cena: %s\n", nome.c_str());
}

// ---------------------------------------------------------------------------------------------
// Tela principal: LcdDisplay::SetupUI (main/display/lcd_display.cc, sem WeChat e sem multilinha)
// seguida do CustomLcdDisplay::SetupUI do Watcher (aplicar_patches.py, seção da placa)
// ---------------------------------------------------------------------------------------------
class Tela {
public:
    lv_obj_t* top_bar_ = nullptr;
    lv_obj_t* status_bar_ = nullptr;
    lv_obj_t* container_ = nullptr;
    lv_obj_t* bottom_bar_ = nullptr;
    lv_obj_t* preview_image_ = nullptr;
    lv_obj_t* emoji_label_ = nullptr;
    lv_obj_t* emoji_image_ = nullptr;
    lv_obj_t* emoji_box_ = nullptr;
    lv_obj_t* chat_message_label_ = nullptr;
    lv_obj_t* network_label_ = nullptr;
    lv_obj_t* mute_label_ = nullptr;
    lv_obj_t* battery_label_ = nullptr;
    lv_obj_t* status_label_ = nullptr;
    lv_obj_t* notification_label_ = nullptr;
    lv_obj_t* low_battery_popup_ = nullptr;
    lv_obj_t* low_battery_label_ = nullptr;
    std::unique_ptr<LvglGif> gif_controller_;
    std::map<std::string, std::vector<uint8_t>> gifs_;
    std::map<std::string, lv_image_dsc_t> dscs_;
    bool hide_subtitle_ = false;
    int spacing(int n) const { return 2 * n; }

    void SetupBase() {
        auto text_font = &font_noto_sans_basic_30_4;
        auto icon_font = &font_material_symbols_20_4;
        auto large_icon_font = &font_material_symbols_30_4;
        lv_color_t bg = lv_color_hex(0x000000), fg = lv_color_hex(0xFFFFFF);

        auto screen = lv_screen_active();
        lv_obj_set_style_text_font(screen, text_font, 0);
        lv_obj_set_style_text_color(screen, fg, 0);
        lv_obj_set_style_bg_color(screen, bg, 0);

        container_ = lv_obj_create(screen);
        lv_obj_set_size(container_, LV_HOR_RES, LV_VER_RES);
        lv_obj_set_style_radius(container_, 0, 0);
        lv_obj_set_style_pad_all(container_, 0, 0);
        lv_obj_set_style_border_width(container_, 0, 0);
        lv_obj_set_style_bg_color(container_, bg, 0);
        lv_obj_set_style_border_color(container_, fg, 0);

        emoji_box_ = lv_obj_create(screen);
        lv_obj_set_size(emoji_box_, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(emoji_box_, LV_OPA_TRANSP, 0);
        lv_obj_set_style_pad_all(emoji_box_, 0, 0);
        lv_obj_set_style_border_width(emoji_box_, 0, 0);
        lv_obj_align(emoji_box_, LV_ALIGN_CENTER, 0, 0);

        emoji_label_ = lv_label_create(emoji_box_);
        lv_obj_set_style_text_font(emoji_label_, large_icon_font, 0);
        lv_obj_set_style_text_color(emoji_label_, fg, 0);
        lv_label_set_text(emoji_label_, MATERIAL_SYMBOLS_ROBOT_2);

        emoji_image_ = lv_image_create(emoji_box_);
        lv_obj_center(emoji_image_);
        lv_obj_add_flag(emoji_image_, LV_OBJ_FLAG_HIDDEN);

        preview_image_ = lv_image_create(screen);
        lv_obj_set_size(preview_image_, kLado / 2, kLado / 2);
        lv_obj_align(preview_image_, LV_ALIGN_CENTER, 0, 0);
        lv_obj_add_flag(preview_image_, LV_OBJ_FLAG_HIDDEN);

        top_bar_ = lv_obj_create(screen);
        lv_obj_set_size(top_bar_, LV_HOR_RES, LV_SIZE_CONTENT);
        lv_obj_set_style_radius(top_bar_, 0, 0);
        lv_obj_set_style_bg_opa(top_bar_, LV_OPA_50, 0);
        lv_obj_set_style_bg_color(top_bar_, bg, 0);
        lv_obj_set_style_border_width(top_bar_, 0, 0);
        lv_obj_set_style_pad_all(top_bar_, 0, 0);
        lv_obj_set_style_pad_top(top_bar_, spacing(2), 0);
        lv_obj_set_style_pad_bottom(top_bar_, spacing(2), 0);
        lv_obj_set_style_pad_left(top_bar_, spacing(4), 0);
        lv_obj_set_style_pad_right(top_bar_, spacing(4), 0);
        lv_obj_set_flex_flow(top_bar_, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(top_bar_, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_scrollbar_mode(top_bar_, LV_SCROLLBAR_MODE_OFF);
        lv_obj_align(top_bar_, LV_ALIGN_TOP_MID, 0, 0);

        network_label_ = lv_label_create(top_bar_);
        lv_label_set_text(network_label_, MATERIAL_SYMBOLS_WIFI);
        lv_obj_set_style_text_font(network_label_, icon_font, 0);
        lv_obj_set_style_text_color(network_label_, fg, 0);

        lv_obj_t* right_icons = lv_obj_create(top_bar_);
        lv_obj_set_size(right_icons, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(right_icons, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(right_icons, 0, 0);
        lv_obj_set_style_pad_all(right_icons, 0, 0);
        lv_obj_set_flex_flow(right_icons, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(right_icons, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        mute_label_ = lv_label_create(right_icons);
        lv_label_set_text(mute_label_, "");
        lv_obj_set_style_text_font(mute_label_, icon_font, 0);
        lv_obj_set_style_text_color(mute_label_, fg, 0);

        battery_label_ = lv_label_create(right_icons);
        lv_label_set_text(battery_label_, MATERIAL_SYMBOLS_BATTERY_ANDROID_FRAME_6);
        lv_obj_set_style_text_font(battery_label_, icon_font, 0);
        lv_obj_set_style_text_color(battery_label_, fg, 0);
        lv_obj_set_style_margin_left(battery_label_, spacing(2), 0);

        status_bar_ = lv_obj_create(screen);
        lv_obj_set_size(status_bar_, LV_HOR_RES, LV_SIZE_CONTENT);
        lv_obj_set_style_radius(status_bar_, 0, 0);
        lv_obj_set_style_bg_opa(status_bar_, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(status_bar_, 0, 0);
        lv_obj_set_style_pad_all(status_bar_, 0, 0);
        lv_obj_set_style_pad_top(status_bar_, spacing(2), 0);
        lv_obj_set_style_pad_bottom(status_bar_, spacing(2), 0);
        lv_obj_set_scrollbar_mode(status_bar_, LV_SCROLLBAR_MODE_OFF);
        lv_obj_set_style_layout(status_bar_, LV_LAYOUT_NONE, 0);
        lv_obj_align(status_bar_, LV_ALIGN_TOP_MID, 0, 0);

        notification_label_ = lv_label_create(status_bar_);
        lv_obj_set_width(notification_label_, LV_HOR_RES * 0.75);
        lv_obj_set_style_text_align(notification_label_, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(notification_label_, fg, 0);
        lv_label_set_text(notification_label_, "");
        lv_obj_align(notification_label_, LV_ALIGN_CENTER, 0, 0);
        lv_obj_add_flag(notification_label_, LV_OBJ_FLAG_HIDDEN);

        status_label_ = lv_label_create(status_bar_);
        lv_obj_set_width(status_label_, LV_HOR_RES * 0.75);
        lv_label_set_long_mode(status_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
        lv_obj_set_style_text_align(status_label_, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(status_label_, fg, 0);
        lv_label_set_text(status_label_, "Inicializando...");
        lv_obj_align(status_label_, LV_ALIGN_CENTER, 0, 0);

        bottom_bar_ = lv_obj_create(screen);
        lv_obj_set_size(bottom_bar_, LV_HOR_RES, text_font->line_height + spacing(8));
        lv_obj_set_style_radius(bottom_bar_, 0, 0);
        lv_obj_set_style_bg_color(bottom_bar_, bg, 0);
        lv_obj_set_style_text_color(bottom_bar_, fg, 0);
        lv_obj_set_style_pad_all(bottom_bar_, 0, 0);
        lv_obj_set_style_pad_left(bottom_bar_, spacing(4), 0);
        lv_obj_set_style_pad_right(bottom_bar_, spacing(4), 0);
        lv_obj_set_style_border_width(bottom_bar_, 0, 0);
        lv_obj_set_scrollbar_mode(bottom_bar_, LV_SCROLLBAR_MODE_OFF);
        lv_obj_align(bottom_bar_, LV_ALIGN_BOTTOM_MID, 0, 0);

        chat_message_label_ = lv_label_create(bottom_bar_);
        lv_label_set_text(chat_message_label_, "");
        lv_obj_set_width(chat_message_label_, LV_HOR_RES - spacing(8));
        lv_label_set_long_mode(chat_message_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
        lv_obj_set_style_text_align(chat_message_label_, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(chat_message_label_, fg, 0);
        lv_obj_align(chat_message_label_, LV_ALIGN_CENTER, 0, 0);
        lv_obj_add_flag(bottom_bar_, LV_OBJ_FLAG_HIDDEN);

        low_battery_popup_ = lv_obj_create(screen);
        lv_obj_set_scrollbar_mode(low_battery_popup_, LV_SCROLLBAR_MODE_OFF);
        lv_obj_set_size(low_battery_popup_, LV_HOR_RES * 0.9, text_font->line_height * 2);
        lv_obj_align(low_battery_popup_, LV_ALIGN_BOTTOM_MID, 0, -spacing(4));
        lv_obj_set_style_bg_color(low_battery_popup_, lv_color_hex(0xFF0000), 0);
        lv_obj_set_style_radius(low_battery_popup_, spacing(4), 0);
        low_battery_label_ = lv_label_create(low_battery_popup_);
        lv_label_set_text(low_battery_label_, "Bateria fraca");
        lv_obj_center(low_battery_label_);
        lv_obj_add_flag(low_battery_popup_, LV_OBJ_FLAG_HIDDEN);
    }

    // CustomLcdDisplay::SetupUI (sensecap_watcher.cc depois dos patches)
    void SetupWatcher() {
        auto text_font = &font_noto_sans_basic_30_4;
        auto icon_font = &font_material_symbols_20_4;
        lv_obj_set_size(top_bar_, LV_HOR_RES, text_font->line_height);
        lv_obj_set_style_layout(top_bar_, LV_LAYOUT_NONE, 0);
        lv_obj_set_style_pad_top(top_bar_, 10, 0);
        lv_obj_set_style_pad_bottom(top_bar_, 1, 0);
        lv_obj_set_size(status_bar_, LV_HOR_RES, text_font->line_height);
        lv_obj_set_style_layout(status_bar_, LV_LAYOUT_NONE, 0);
        lv_obj_set_style_pad_top(status_bar_, 10, 0);
        lv_obj_set_style_pad_bottom(status_bar_, 1, 0);
        lv_obj_set_y(status_bar_, text_font->line_height);
        lv_obj_add_flag(status_bar_, LV_OBJ_FLAG_IGNORE_LAYOUT);
        lv_obj_set_parent(mute_label_, top_bar_);
        lv_obj_set_parent(battery_label_, top_bar_);
        lv_obj_set_style_margin_left(battery_label_, 0, 0);
        lv_obj_align(network_label_, LV_ALIGN_TOP_MID, -1.5 * icon_font->line_height, 0);
        lv_obj_align(mute_label_, LV_ALIGN_TOP_MID, 1.0 * icon_font->line_height, 0);
        lv_obj_align(battery_label_, LV_ALIGN_TOP_MID, 2.5 * icon_font->line_height, 0);
        lv_obj_align(status_label_, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_set_flex_grow(status_label_, 0);
        lv_obj_set_width(status_label_, LV_HOR_RES * 0.75);
        lv_label_set_long_mode(status_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
        lv_obj_align(notification_label_, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_set_width(notification_label_, LV_HOR_RES * 0.75);
        lv_label_set_long_mode(notification_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
        lv_obj_align(low_battery_popup_, LV_ALIGN_BOTTOM_MID, 0, -20);
        lv_obj_set_width(low_battery_label_, LV_HOR_RES * 0.75);
        lv_label_set_long_mode(low_battery_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
        AplicarLayoutRedondo();
        // UpdateStatusBar com bateria: ícones mais à esquerda e o percentual
        int l = icon_font->line_height;
        lv_obj_align(network_label_, LV_ALIGN_TOP_MID, -5 * l / 2, 0);
        lv_obj_align(mute_label_, LV_ALIGN_TOP_MID, -l / 2, 0);
        lv_obj_align(battery_label_, LV_ALIGN_TOP_MID, 3 * l / 2, 0);
        lv_obj_set_style_text_font(network_label_, &font_material_symbols_16_4, 0);
        auto pct = lv_label_create(top_bar_);
        lv_obj_set_style_text_font(pct, Fontes::Pequena(), 0);
        lv_label_set_text(pct, "87%");
        lv_obj_align_to(pct, battery_label_, LV_ALIGN_OUT_RIGHT_MID, 4, 0);
    }

    static constexpr int kTextoLargura = 290;
    static constexpr int kLinhasVisiveis = 3;
    std::string papel_atual_;

    void AplicarLayoutRedondo() {
        lv_obj_set_size(bottom_bar_, kTextoLargura, lv_font_get_line_height(Fontes::Grande()) * kLinhasVisiveis + 4);
        lv_obj_set_style_bg_opa(bottom_bar_, LV_OPA_TRANSP, 0);
        lv_obj_set_style_pad_all(bottom_bar_, 0, 0);
        lv_obj_add_flag(bottom_bar_, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_scroll_dir(bottom_bar_, LV_DIR_VER);
        lv_obj_set_scrollbar_mode(bottom_bar_, LV_SCROLLBAR_MODE_OFF);
        lv_obj_set_width(chat_message_label_, kTextoLargura);
        lv_obj_set_height(chat_message_label_, LV_SIZE_CONTENT);
        lv_label_set_long_mode(chat_message_label_, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_font(chat_message_label_, Fontes::Grande(), 0);
        lv_obj_set_style_text_line_space(chat_message_label_, 0, 0);
        lv_obj_set_style_text_align(chat_message_label_, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(chat_message_label_, LV_ALIGN_TOP_MID, 0, 0);
        lv_obj_set_style_text_font(status_label_, Fontes::Pequena(), 0);
        lv_obj_set_style_text_font(notification_label_, Fontes::Pequena(), 0);
        PosicionarMascote();
    }

    int AlturaCaixaTexto() const { return lv_font_get_line_height(Fontes::Grande()) * kLinhasVisiveis + 4; }

    // Igual ao CustomLcdDisplay::PosicionarMascote (aplicar_patches.py)
    void PosicionarMascote() {
        LayoutMascote::Aplicar(emoji_box_, emoji_image_, bottom_bar_, chat_message_label_, papel_atual_,
                               hide_subtitle_, AlturaCaixaTexto());
    }

    void RolarParaFim() {
        lv_obj_update_layout(bottom_bar_);
        int32_t falta = lv_obj_get_scroll_bottom(bottom_bar_);
        if (falta > 0) {
            lv_obj_scroll_by(bottom_bar_, 0, -falta, LV_ANIM_OFF);
        }
    }

    // SetChatMessage do Watcher, sem o streaming (a fala aparece inteira)
    void SetChatMessage(const std::string& papel, const std::string& texto) {
        if (texto.empty()) {
            papel_atual_.clear();
            lv_label_set_text(chat_message_label_, "");
            lv_obj_add_flag(bottom_bar_, LV_OBJ_FLAG_HIDDEN);
            PosicionarMascote();
            return;
        }
        papel_atual_ = papel;
        lv_obj_scroll_to_y(bottom_bar_, 0, LV_ANIM_OFF);
        lv_label_set_text(chat_message_label_, texto.c_str());
        RolarParaFim();
        if (!hide_subtitle_) {
            lv_obj_remove_flag(bottom_bar_, LV_OBJ_FLAG_HIDDEN);
        }
        PosicionarMascote();
    }

    void SetHideSubtitle(bool hide) {
        hide_subtitle_ = hide;
        if (hide) {
            lv_obj_add_flag(bottom_bar_, LV_OBJ_FLAG_HIDDEN);
        }
        PosicionarMascote();
    }

    void SetStatus(const char* s) { lv_label_set_text(status_label_, s); }

    // Volume na roda: igual ao CustomLcdDisplay (aplicar_patches.py)
    std::string emocao_atual_ = "neutral";
    int volume_sentido_ = 0;
    lv_timer_t* timer_volume_ = nullptr;

    void SetEmotion(const std::string& emocao) {
        emocao_atual_ = emocao;
        if (volume_sentido_ == 0) {
            TrocarGif(emocao);
        }
    }

    AnelVolume anel_volume_;

    void MostrarVolume(bool aumentando, int volume) {
        lv_label_set_text_fmt(status_label_, "Volume: %d", volume);  // no aparelho é a notificação do topo
        anel_volume_.Mostrar(volume);
        int sentido = aumentando ? 1 : -1;
        if (sentido != volume_sentido_) {
            volume_sentido_ = sentido;
            TrocarGif(aumentando ? "volume_mais" : "volume_menos");
        }
        if (timer_volume_ == nullptr) {
            timer_volume_ = lv_timer_create(
                [](lv_timer_t* timer) {
                    auto self = static_cast<Tela*>(lv_timer_get_user_data(timer));
                    lv_timer_pause(timer);
                    self->volume_sentido_ = 0;
                    self->anel_volume_.Esconder();
                    self->TrocarGif(self->emocao_atual_);
                },
                1200, this);
        }
        lv_timer_reset(timer_volume_);
        lv_timer_resume(timer_volume_);
    }

    // LcdDisplay::SetEmotion: GIF do mascote-clawd/emocoes; sem GIF, o emoji da fonte
    void TrocarGif(const std::string& emocao) {
        auto it = dscs_.find(emocao);
        if (it == dscs_.end()) {
            std::ifstream f(Gif(emocao), std::ios::binary);
            if (!f) {
                gif_controller_.reset();
                lv_label_set_text(emoji_label_, MATERIAL_SYMBOLS_ROBOT_2);
                lv_obj_add_flag(emoji_image_, LV_OBJ_FLAG_HIDDEN);
                lv_obj_remove_flag(emoji_label_, LV_OBJ_FLAG_HIDDEN);
                return;
            }
            gifs_[emocao] = std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
            lv_image_dsc_t d = {};
            d.data = gifs_[emocao].data();
            d.data_size = gifs_[emocao].size();
            it = dscs_.emplace(emocao, d).first;
        }
        if (gif_controller_) {
            gif_controller_->Stop();
            gif_controller_.reset();
        }
        gif_controller_ = std::make_unique<LvglGif>(&it->second);
        if (gif_controller_->IsLoaded()) {
            gif_controller_->SetFrameCallback([this]() { lv_image_set_src(emoji_image_, gif_controller_->image_dsc()); });
            lv_image_set_src(emoji_image_, gif_controller_->image_dsc());
            gif_controller_->Start();
            lv_obj_add_flag(emoji_label_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(emoji_image_, LV_OBJ_FLAG_HIDDEN);
        }
    }
};

// Todas as poses do mascote-clawd/emocoes, em ordem alfabética
static std::vector<std::string> Poses() {
    std::vector<std::string> r;
    if (auto d = opendir("../mascote-clawd/emocoes")) {
        while (auto e = readdir(d)) {
            std::string n = e->d_name;
            if (n.size() > 4 && n.substr(n.size() - 4) == ".gif") {
                r.push_back(n.substr(0, n.size() - 4));
            }
        }
        closedir(d);
    }
    std::sort(r.begin(), r.end());
    return r;
}

// GIF do mascote-clawd lido do disco (no aparelho vem da partição de assets)
class GifArquivo : public LvglImage {
public:
    explicit GifArquivo(const std::string& caminho) {
        std::ifstream f(caminho, std::ios::binary);
        dados_.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
        dsc_ = {};
        dsc_.data = dados_.data();
        dsc_.data_size = dados_.size();
    }
    const lv_img_dsc_t* image_dsc() const override { return &dsc_; }
    bool IsGif() const override { return true; }

private:
    std::vector<uint8_t> dados_;
    lv_image_dsc_t dsc_;
};

int main() {
    lv_init();
    lv_tick_set_cb([]() -> uint32_t { return g_ms; });
    auto disp = lv_display_create(kLado, kLado);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(disp, g_fb, nullptr, sizeof(g_fb), LV_DISPLAY_RENDER_MODE_DIRECT);
    lv_display_set_flush_cb(disp, Flush);

    Tela t;
    t.SetupBase();
    t.SetupWatcher();
    g_caixa_texto = t.bottom_bar_;
    AberturaWatcher::Mostrar();
    Avancar(600);
    SalvarPng("00-abertura");
    Avancar(4000);  // a abertura some sozinha

    // Fluxos na ordem em que aparecem no aparelho
    t.SetStatus("");
    t.SetEmotion("conectando");
    t.SetChatMessage("carregando", "Verificando atualização...");
    Avancar(200);
    SalvarPng("01-carregando");

    t.SetChatMessage("system", "");
    t.SetStatus("Seg, 05/10 · 16:20");
    t.SetEmotion("neutral");
    Avancar(200);
    SalvarPng("02-espera-sem-frase");

    t.SetEmotion("happy");
    t.SetChatMessage("saudacao", "Boa tarde, Ana! Café já tomado?");
    Avancar(200);
    SalvarPng("03-espera-saudacao");

    t.SetEmotion("relaxed");
    t.SetChatMessage("saudacao", "Boa noite, Ana! Hora de desligar e descansar um pouco?");
    Avancar(200);
    SalvarPng("04-espera-saudacao-longa");

    t.SetEmotion("codando");
    t.SetChatMessage("saudacao", "* Codando…\nxiaozhi-ollie +2");
    Avancar(200);
    SalvarPng("05-espera-trabalhando");

    t.SetStatus("Conectando...");
    t.SetEmotion("conectando");
    t.SetChatMessage("system", "");
    Avancar(200);
    SalvarPng("06-conectando");

    t.SetStatus("Ouvindo...");
    t.SetEmotion("ouvindo");
    Avancar(200);
    SalvarPng("07-ouvindo");

    t.SetChatMessage("user", "Como estão as sessões?");
    Avancar(200);
    SalvarPng("08-ouvindo-com-texto");

    t.SetStatus("Falando...");
    t.SetEmotion("falando");
    t.SetChatMessage("assistant", "Tem três sessões rodando e uma esperando você no xiaozhi-ollie.");
    Avancar(200);
    SalvarPng("09-falando");

    t.SetHideSubtitle(true);
    Avancar(200);
    SalvarPng("10-falando-sem-legenda");
    t.SetHideSubtitle(false);

    t.SetChatMessage("system", "");
    t.SetStatus("Seg, 05/10 · 16:21");
    t.SetEmotion("winking");
    Avancar(200);
    SalvarPng("11-volta-espera");

    // Roda: anti-horário aumenta, horário diminui; 1,2 s depois do último passo volta a emoção de antes
    for (int v = 55; v <= 75; v += 5) {
        t.MostrarVolume(true, v);
        Avancar(150);
    }
    Avancar(130);  // quadro par (sem o pulinho de propósito do "volume_mais")
    SalvarPng("12-volume-aumentando");
    for (int v = 70; v >= 40; v -= 5) {
        t.MostrarVolume(false, v);
        Avancar(150);
    }
    SalvarPng("13-volume-diminuindo");
    t.SetEmotion("happy");  // emoção que chega enquanto gira: aparece quando a roda para
    Avancar(1300);
    t.SetStatus("Seg, 05/10 · 16:22");
    SalvarPng("14-volume-parou");

    // Telas de carregamento dos apps: o Clawd no lugar do spinner (o mesmo PainelWatcher do firmware)
    LvglTheme tema;
    auto colecao = std::make_shared<EmojiCollection>();
    for (auto& pose : Poses()) {
        colecao->AddEmoji(pose, new GifArquivo(Gif(pose)));
    }
    tema.set_emoji_collection(colecao);
    Display tela_apps;
    tela_apps.current_theme_ = &tema;
    PainelWatcher painel(&tela_apps);
    g_painel_aberto = true;
    painel.MostrarStatus("Conversas", PainelWatcher::Status::Carregando, "Buscando conversas…");
    Avancar(300);
    SalvarPng("15-app-buscando");
    painel.MostrarStatus("Claude Code", PainelWatcher::Status::Carregando, "Lendo as mensagens…", {}, "reading");
    Avancar(300);
    SalvarPng("16-app-lendo");
    painel.MostrarStatus("Fazer backup", PainelWatcher::Status::Carregando, "Fazendo backup no Mac…", {}, "skate");
    Avancar(300);
    SalvarPng("17-app-backup");
    painel.MostrarStatus("Gravador", PainelWatcher::Status::Carregando, "Salvando a gravação…", {"Cancelar"}, "recording");
    Avancar(300);
    SalvarPng("18-app-com-botoes");
    painel.MostrarStatus("Fazer backup", PainelWatcher::Status::Sucesso, "Backup concluído");
    Avancar(300);
    SalvarPng("19-app-sucesso");
    // Configurações > Atualização (placa/apps/app_configuracoes.h)
    painel.MostrarStatus("Atualização", PainelWatcher::Status::Carregando, "Procurando atualização…", {}, "searching");
    Avancar(300);
    SalvarPng("21-atualizacao-procurando");
    painel.MostrarTexto("Atualização",
                        "Versão nova: 2.5.1 (você está na 2.5.0).\n\nO Watcher baixa, instala e reinicia sozinho. "
                        "Não desligue até terminar.",
                        {"Atualizar agora", "Agora não"});
    Avancar(300);
    SalvarPng("22-atualizacao-pronta-sucesso");  // "-sucesso": tela sem Clawd, o medir.py não mede
    painel.Fechar();

    // Modo de configuração de Wi-Fi (o firmware chama MostrarSemWifi com a rede e o endereço do portal)
    TelaSemWifi sem_wifi(&tela_apps);
    sem_wifi.Mostrar("Ollie-AC40", "http://192.168.4.1");
    Avancar(1100);  // quadro com o Wi-Fi riscado
    SalvarPng("20-sem-wifi");
    sem_wifi.Esconder();
    g_painel_aberto = false;
    Avancar(100);

    // Cada pose na espera, para conferir as animações novas (folha à parte, sem medir)
    for (auto& pose : Poses()) {
        t.SetEmotion(pose);
        Avancar(100);
        SalvarPng("p-" + pose);
    }
    return 0;
}

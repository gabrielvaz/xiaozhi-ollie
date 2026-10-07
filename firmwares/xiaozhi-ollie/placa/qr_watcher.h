// QR code num canvas do LVGL, com módulos inteiros (nítido) e borda de 2 módulos; usado pelo app
// "Mostrar QR code" (painel) e pela tela sem Wi-Fi. Quem chama trava a tela e libera o buffer.
#pragma once

#include <lvgl.h>

#include <algorithm>
#include <cstdint>
#include <string>

// qrcodegen (Nayuki, MIT) já vem no componente esp_emote_gfx; ativar o lv_qrcode duplicaria os símbolos
extern "C" {
bool qrcodegen_encodeText(const char* text, uint8_t tempBuffer[], uint8_t qrcode[], int ecl, int minVersion,
                          int maxVersion, int mask, bool boostEcl);
int qrcodegen_getSize(const uint8_t qrcode[]);
bool qrcodegen_getModule(const uint8_t qrcode[], int x, int y);
}

// Desenha o QR em pai, com no máximo lado_max px; buf recebe a imagem (destrua com lv_draw_buf_destroy ao
// trocar de tela). Retorna nullptr se o texto não cabe (até a versão 10 = 57x57 módulos).
inline lv_obj_t* DesenharQr(lv_obj_t* pai, const std::string& conteudo, int lado_max, lv_draw_buf_t*& buf) {
    static uint8_t qr[((10 * 4 + 17) * (10 * 4 + 17) + 7) / 8 + 1];
    static uint8_t temporario[sizeof(qr)];
    if (!qrcodegen_encodeText(conteudo.c_str(), temporario, qr, 1 /* Ecc MEDIUM */, 1, 10, -1 /* máscara auto */, true)) {
        return nullptr;
    }
    int n = qrcodegen_getSize(qr);
    int escala = std::max(1, lado_max / (n + 4));  // borda de 2 módulos de cada lado
    int lado = (n + 4) * escala;
    if (buf != nullptr) {
        lv_draw_buf_destroy(buf);
    }
    buf = lv_draw_buf_create(lado, lado, LV_COLOR_FORMAT_RGB565, 0);
    auto canvas = lv_canvas_create(pai);
    lv_canvas_set_draw_buf(canvas, buf);
    lv_canvas_fill_bg(canvas, lv_color_white(), LV_OPA_COVER);
    for (int y = 0; y < n; y++) {
        for (int x = 0; x < n; x++) {
            if (!qrcodegen_getModule(qr, x, y)) {
                continue;
            }
            for (int dy = 0; dy < escala; dy++) {
                for (int dx = 0; dx < escala; dx++) {
                    lv_canvas_set_px(canvas, (x + 2) * escala + dx, (y + 2) * escala + dy, lv_color_black(), LV_OPA_COVER);
                }
            }
        }
    }
    return canvas;
}

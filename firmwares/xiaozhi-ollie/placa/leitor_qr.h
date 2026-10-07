// Leitura de QR code numa foto da câmera (RGB565, 640x480 do Himax) com o quirc (ISC, baixado pelo
// compilar.sh em quirc/) e interpretação do QR de Wi-Fi ("WIFI:T:WPA;S:rede;P:senha;;", o que o iPhone e
// o Android geram ao compartilhar uma rede).
#pragma once

#include <cstdint>
#include <cstring>
#include <string>

#include "quirc.h"

namespace LeitorQr {

// Procura um QR na imagem; retorna o texto do primeiro que decodificar (vazio se não achou)
inline std::string Ler(const uint16_t* rgb565, int largura, int altura) {
    static quirc* q = nullptr;  // reaproveitado entre as fotos (a imagem cinza fica na PSRAM)
    if (q == nullptr) {
        q = quirc_new();
        if (q == nullptr) {
            return "";
        }
    }
    int w = 0, h = 0;
    quirc_begin(q, &w, &h);
    if (w != largura || h != altura) {
        if (quirc_resize(q, largura, altura) < 0) {
            return "";
        }
    }
    uint8_t* cinza = quirc_begin(q, nullptr, nullptr);
    for (int i = 0; i < largura * altura; i++) {
        uint16_t p = rgb565[i];
        int r = (p >> 11) & 0x1F, g = (p >> 5) & 0x3F, b = p & 0x1F;
        cinza[i] = (uint8_t)((r * 527 * 77 + g * 259 * 150 + b * 527 * 29) >> 14);  // luma (0..255)
    }
    quirc_end(q);
    for (int i = 0; i < quirc_count(q); i++) {
        quirc_code codigo;
        quirc_data dados;
        quirc_extract(q, i, &codigo);
        quirc_decode_error_t erro = quirc_decode(&codigo, &dados);
        if (erro == QUIRC_ERROR_DATA_ECC) {
            quirc_flip(&codigo);  // foto espelhada
            erro = quirc_decode(&codigo, &dados);
        }
        if (erro == QUIRC_SUCCESS) {
            return std::string(reinterpret_cast<const char*>(dados.payload), dados.payload_len);
        }
    }
    return "";
}

struct RedeWifi {
    std::string ssid;
    std::string senha;
    bool valida = false;
};

// "WIFI:T:WPA;S:Minha rede;P:segredo;H:false;;" -> {ssid, senha}. Campos em qualquer ordem;
// \ escapa ; , : \ e aspas
inline RedeWifi InterpretarWifi(const std::string& texto) {
    RedeWifi rede;
    if (texto.size() < 5 || strncasecmp(texto.c_str(), "WIFI:", 5) != 0) {
        return rede;
    }
    size_t i = 5;
    while (i < texto.size()) {
        size_t dois_pontos = texto.find(':', i);
        if (dois_pontos == std::string::npos) {
            break;
        }
        std::string campo = texto.substr(i, dois_pontos - i);
        std::string valor;
        size_t j = dois_pontos + 1;
        for (; j < texto.size() && texto[j] != ';'; j++) {
            if (texto[j] == '\\' && j + 1 < texto.size()) {
                j++;
            }
            valor += texto[j];
        }
        if (campo == "S") {
            rede.ssid = valor;
        } else if (campo == "P") {
            rede.senha = valor;
        }
        i = j + 1;
        if (i < texto.size() && texto[i] == ';') {
            break;  // ";;" fecha
        }
    }
    rede.valida = !rede.ssid.empty() && rede.ssid.size() <= 32 && rede.senha.size() <= 64;
    return rede;
}

}  // namespace LeitorQr

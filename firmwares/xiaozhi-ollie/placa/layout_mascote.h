// Onde o Clawd e o texto ficam na tela redonda (412x412), com a mesma regra em todos os fluxos:
// o corpo do Clawd fica sempre no centro exato da tela, e o texto (se houver) vai logo abaixo dele.
// As 3 linhas da conversa cabem no círculo abaixo do centro (a 345 px a corda ainda tem ~300 px).
// Na horizontal o corpo já é simétrico no GIF (mascote-clawd/gerar.py), então basta centrar a imagem.
// Usado pelo firmware (CustomLcdDisplay) e pelo simulador (../simulador), para os dois baterem.
#pragma once

#include <lvgl.h>

#include <algorithm>
#include <string>

namespace LayoutMascote {

constexpr int kLado = 412;
constexpr int kGif = 128;                     // GIFs do mascote-clawd: 128x128
constexpr int kCorpoTopo = 34;                // corpo + perninhas dentro do GIF (gerar.py: grade 20x18,
constexpr int kCorpoBase = 100;               // PX = 6, corpo da linha 4 às perninhas na linha 15)
constexpr int kEscalaCheia = 256;             // espera, saudação, carregando
constexpr int kEscalaConversa = 220;          // conversa: Clawd um pouco menor para caber 3 linhas
constexpr int kVao = 22;                      // entre as perninhas e o texto

struct Posicao {
    int mascote_dy;  // deslocamento do centro do GIF em relação ao centro da tela (LV_ALIGN_CENTER)
    int texto_y;     // topo do texto (LV_ALIGN_TOP_MID)
};

inline Posicao Calcular(int escala) {
    int corpo = (kCorpoBase - kCorpoTopo) * escala / 256;
    int desvio = (((kCorpoTopo + kCorpoBase) / 2 - kGif / 2) * escala + 128) / 256;  // corpo abaixo do centro do GIF (arredondado)
    int topo = kLado / 2 - corpo / 2;
    Posicao p;
    p.mascote_dy = -desvio;
    p.texto_y = topo + corpo + kVao;
    return p;
}

// Reposiciona o Clawd (emoji_box_/emoji_image_ do XiaoZhi) e a caixa do texto (bottom_bar_ com o label).
// papel: "" (sem texto), "saudacao" (espera), "carregando" ("Verificando atualização") ou conversa.
// Na conversa a caixa tem 3 linhas e o Clawd fica um pouco menor; saudação e carregando usam a altura
// real do texto.
inline void Aplicar(lv_obj_t* emoji_box, lv_obj_t* emoji_image, lv_obj_t* caixa, lv_obj_t* label,
                    const std::string& papel, bool texto_oculto, int altura_caixa) {
    if (emoji_box == nullptr || caixa == nullptr || label == nullptr) {
        return;
    }
    bool conversa = papel != "saudacao" && papel != "carregando";
    int altura = 0;
    if (!papel.empty() && !texto_oculto) {
        if (conversa) {
            altura = altura_caixa;
        } else {
            lv_obj_update_layout(caixa);
            altura = std::min<int>(lv_obj_get_height(label), altura_caixa);
        }
    }
    int escala = (altura > 0 && conversa) ? kEscalaConversa : kEscalaCheia;
    Posicao p = Calcular(escala);
    lv_obj_align(emoji_box, LV_ALIGN_CENTER, 0, p.mascote_dy);
    if (emoji_image != nullptr) {
        lv_image_set_scale(emoji_image, escala);
    }
    lv_obj_set_height(caixa, altura > 0 ? altura : altura_caixa);
    lv_obj_align(caixa, LV_ALIGN_TOP_MID, 0, p.texto_y);
}

}  // namespace LayoutMascote

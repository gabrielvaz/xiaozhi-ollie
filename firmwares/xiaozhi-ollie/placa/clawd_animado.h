// Clawd animado fora da tela principal (telas de carregamento dos apps, tela sem Wi-Fi): toca um GIF do
// mascote-clawd lido da coleção de emojis do tema (partição de assets). Quem chama trava a tela.
#pragma once

#include <lvgl.h>

#include <memory>

#include "display.h"
#include "gif/lvgl_gif.h"
#include "lvgl_theme.h"

class ClawdAnimado {
public:
    explicit ClawdAnimado(Display* display) : display_(display) {}
    ~ClawdAnimado() { Parar(); }

    // Cria a imagem em pai, tocando a pose. Sem o GIF (assets ainda não carregados), retorna nullptr
    lv_obj_t* Criar(lv_obj_t* pai, const char* pose) {
        Parar();
        auto tema = static_cast<LvglTheme*>(display_->GetTheme());
        auto colecao = tema != nullptr ? tema->emoji_collection() : nullptr;
        auto imagem = colecao != nullptr ? colecao->GetEmojiImage(pose) : nullptr;
        if (imagem == nullptr || !imagem->IsGif()) {
            return nullptr;
        }
        gif_ = std::make_unique<LvglGif>(imagem->image_dsc());
        if (!gif_->IsLoaded()) {
            gif_.reset();
            return nullptr;
        }
        img_ = lv_image_create(pai);
        gif_->SetFrameCallback([this]() {
            if (img_ != nullptr) {
                lv_image_set_src(img_, gif_->image_dsc());
            }
        });
        lv_image_set_src(img_, gif_->image_dsc());
        gif_->Start();
        return img_;
    }

    // Para o GIF; chame antes de apagar a tela onde a imagem está (o próximo quadro escreveria nela)
    void Parar() {
        if (gif_) {
            gif_->Stop();
            gif_.reset();
        }
        img_ = nullptr;
    }

private:
    Display* display_;
    std::unique_ptr<LvglGif> gif_;
    lv_obj_t* img_ = nullptr;
};

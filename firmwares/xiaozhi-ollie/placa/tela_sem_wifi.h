// Tela do modo de configuração de Wi-Fi (sem rede salva, a rede não conectou ou o Wi-Fi caiu por 1 min).
// Três páginas, trocadas pela roda (pontinhos embaixo):
//   0. "Sem Wi-Fi", o Clawd confuso ("sem_wifi") com o corpo no centro e, embaixo, a rede do Ollie e o
//      endereço do portal;
//   1. QR da rede do Ollie (aberta): a câmera do celular oferece "Entrar na rede" e o portal abre sozinho
//      (o portal responde às URLs de detecção de portal cativo do iPhone e do Android);
//   2. QR do endereço do portal, para quando o celular não abre sozinho.
// Fica na camada de cima até a configuração terminar (o aparelho reinicia). Quem chama trava a tela.
#pragma once

#include <lvgl.h>

#include <cstring>
#include <string>

#include "clawd_animado.h"
#include "fontes_watcher.h"
#include "idioma_watcher.h"
#include "layout_mascote.h"
#include "qr_watcher.h"

class TelaSemWifi {
public:
    explicit TelaSemWifi(Display* display) : clawd_(display) {}

    void Mostrar(const std::string& rede, const std::string& url) {
        rede_ = rede;
        url_ = url;
        pagina_ = 0;
        Desenhar();
    }

    // Roda girando com a tela à mostra: troca a página. Retorna false se a tela não está à mostra
    bool Girar(bool horario) {
        if (tela_ == nullptr) {
            return false;
        }
        pagina_ = (pagina_ + (horario ? 1 : kPaginas - 1)) % kPaginas;
        Desenhar();
        return true;
    }

    void Esconder() {
        Limpar();
        if (tela_ != nullptr) {
            lv_obj_delete(tela_);
            tela_ = nullptr;
        }
    }

private:
    static constexpr int kPaginas = 3;
    static constexpr uint32_t kLaranja = 0xD97757;
    static constexpr uint32_t kBranco = 0xFFFFFF;
    static constexpr uint32_t kCinza = 0xB4B4B4;

    ClawdAnimado clawd_;
    lv_obj_t* tela_ = nullptr;
    lv_draw_buf_t* qr_buf_ = nullptr;
    std::string rede_, url_;
    int pagina_ = 0;

    // Para o GIF e solta a imagem do QR antes de apagar o conteúdo da página
    void Limpar() {
        clawd_.Parar();
        if (tela_ != nullptr) {
            lv_obj_clean(tela_);
        }
        if (qr_buf_ != nullptr) {
            lv_draw_buf_destroy(qr_buf_);
            qr_buf_ = nullptr;
        }
    }

    void Desenhar() {
        if (tela_ == nullptr) {
            tela_ = lv_obj_create(lv_layer_top());
            lv_obj_remove_style_all(tela_);
            lv_obj_set_size(tela_, LV_PCT(100), LV_PCT(100));
            lv_obj_set_style_bg_color(tela_, lv_color_hex(0x000000), 0);
            lv_obj_set_style_bg_opa(tela_, LV_OPA_COVER, 0);
            lv_obj_remove_flag(tela_, LV_OBJ_FLAG_SCROLLABLE);
        }
        Limpar();
        if (pagina_ == 0) {
            PaginaClawd();
        } else if (pagina_ == 1) {
            PaginaQr(TR("Entre pelo QR", "Join with the QR", "扫码连接", "Entra con el QR"), WifiQr(),
                     TR("Aponte a câmera do celular", "Point your phone camera", "用手机相机扫描", "Apunta la cámara del móvil"),
                     TR("e a configuração abre sozinha", "and setup opens by itself", "设置页面会自动打开", "y la configuración se abre sola"),
                     kCinza);
        } else {
            PaginaQr(TR("Abra no navegador", "Open in the browser", "在浏览器打开", "Abre en el navegador"), url_,
                     TR("Se não abriu sozinha, entre em", "If it didn't open, go to", "如果没有自动打开，请访问", "Si no se abrió, entra en"),
                     Endereco(), kLaranja);
        }
        Pontinhos();
    }

    void PaginaClawd() {
        Linha(TR("Sem Wi-Fi", "No Wi-Fi", "未连接 Wi-Fi", "Sin Wi-Fi"), Fontes::Grande(), kLaranja, 58);
        Linha(TR("Gire: ver QR · Clique: ler QR da rede", "Turn: show QR · Click: scan a network QR", "转动：显示二维码 · 单击：扫码",
                 "Gira: ver QR · Clic: leer QR de la red"),
              Fontes::Pequena(), kCinza, 92);
        // Corpo do Clawd no centro da tela, como nas outras telas (placa/layout_mascote.h)
        auto pos = LayoutMascote::Calcular(LayoutMascote::kEscalaCheia);
        if (auto img = clawd_.Criar(tela_, "sem_wifi")) {
            lv_obj_align(img, LV_ALIGN_CENTER, 0, pos.mascote_dy);
        }
        int y = pos.texto_y;
        y += Linha(TR("No celular, entre na rede", "On your phone, join", "用手机连接网络", "En el móvil, conéctate a"),
                   Fontes::Pequena(), kCinza, y);
        y += Linha(rede_, Fontes::Grande(), kBranco, y) + 4;
        y += Linha(TR("e abra no navegador", "then open in the browser", "然后在浏览器打开", "y abre en el navegador"),
                   Fontes::Pequena(), kCinza, y);
        Linha(Endereco(), Fontes::Grande(), kLaranja, y);
    }

    // QR num quadrado branco no centro, título em cima e duas linhas embaixo
    void PaginaQr(const char* titulo, const std::string& conteudo, const char* linha1, const std::string& linha2,
                  uint32_t cor_linha2) {
        Linha(titulo, Fontes::Grande(), kLaranja, 58);
        auto fundo = lv_obj_create(tela_);
        lv_obj_remove_style_all(fundo);
        lv_obj_set_style_bg_color(fundo, lv_color_white(), 0);
        lv_obj_set_style_bg_opa(fundo, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(fundo, 14, 0);
        lv_obj_remove_flag(fundo, LV_OBJ_FLAG_SCROLLABLE);
        if (auto qr = DesenharQr(fundo, conteudo, 176, qr_buf_)) {
            lv_obj_update_layout(qr);
            int lado = lv_obj_get_width(qr) + 8;
            lv_obj_set_size(fundo, lado, lado);
            lv_obj_center(qr);
        }
        lv_obj_align(fundo, LV_ALIGN_CENTER, 0, -4);
        int y = 314;
        y += Linha(linha1, Fontes::Pequena(), kCinza, y);
        Linha(linha2, pagina_ == 1 ? Fontes::Pequena() : Fontes::Grande(), cor_linha2, y);
    }

    // Uma bolinha por página, a atual em laranja
    void Pontinhos() {
        for (int i = 0; i < kPaginas; i++) {
            auto p = lv_obj_create(tela_);
            lv_obj_remove_style_all(p);
            lv_obj_set_size(p, 8, 8);
            lv_obj_set_style_radius(p, LV_RADIUS_CIRCLE, 0);
            lv_obj_set_style_bg_opa(p, LV_OPA_COVER, 0);
            lv_obj_set_style_bg_color(p, lv_color_hex(i == pagina_ ? kLaranja : 0x555555), 0);
            lv_obj_align(p, LV_ALIGN_TOP_MID, (i - (kPaginas - 1) / 2) * 16, 384);
        }
    }

    // QR de Wi-Fi (padrão WIFI:T:...;S:...;;), rede aberta; escapa os caracteres especiais do nome
    std::string WifiQr() const {
        std::string ssid;
        for (char ch : rede_) {
            if (strchr("\\;,:\"", ch) != nullptr) {
                ssid += '\\';
            }
            ssid += ch;
        }
        return "WIFI:T:nopass;S:" + ssid + ";;";
    }

    // Endereço sem o "http://" e sem a barra final (o celular abre do mesmo jeito e cabe numa linha)
    std::string Endereco() const {
        std::string e = url_;
        for (const char* prefixo : {"http://", "https://"}) {
            if (e.rfind(prefixo, 0) == 0) {
                e = e.substr(strlen(prefixo));
            }
        }
        if (!e.empty() && e.back() == '/') {
            e.pop_back();
        }
        return e;
    }

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

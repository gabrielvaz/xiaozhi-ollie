// Asterisco animado do Claude Code, antes de "Codando…", "Refatorando…" na tela de espera: como o spinner
// do CLI, vai e volta por · ✢ ✳ ✶ ✻ ✽ (cresce, vira +, ganha pontas) a cada 120 ms, girando um pouco a
// cada quadro. As fontes do Watcher não têm esses glifos: cada quadro é desenhado em vetor (raios com
// pontas redondas). O texto marca o lugar com "* " no começo; quem chama trava a tela.
#pragma once

#include <lvgl.h>

#include <cstring>
#include <string>

class AsteriscoClaude {
public:
    static constexpr const char* kMarca = "* ";      // começo do texto que pede o asterisco
    static constexpr const char* kReserva = "    ";  // espaços no lugar da marca (o desenho fica por cima)
    static constexpr int kLado = 26;

    ~AsteriscoClaude() { Esconder(); }

    // Troca a marca pelos espaços; retorna se o texto pedia o asterisco
    static bool PrepararTexto(std::string& texto) {
        if (texto.rfind(kMarca, 0) != 0) {
            return false;
        }
        texto = kReserva + texto.substr(2);
        return true;
    }

    // Mostra o asterisco à esquerda da 1ª letra depois da reserva, dentro do label (rola junto com ele)
    void Mostrar(lv_obj_t* label) {
        if (obj_ == nullptr || lv_obj_get_parent(obj_) != label) {
            Esconder();
            obj_ = lv_obj_create(label);
            lv_obj_remove_style_all(obj_);
            lv_obj_set_size(obj_, kLado, kLado);
            lv_obj_add_flag(obj_, LV_OBJ_FLAG_IGNORE_LAYOUT);
            lv_obj_remove_flag(obj_, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_event_cb(obj_, Desenhar, LV_EVENT_DRAW_MAIN, this);
            timer_ = lv_timer_create(Avancar, kQuadroMs, this);
        }
        lv_obj_update_layout(label);
        lv_point_t letra;
        lv_label_get_letter_pos(label, strlen(kReserva), &letra);
        auto fonte = lv_obj_get_style_text_font(label, LV_PART_MAIN);
        int altura = fonte != nullptr ? lv_font_get_line_height(fonte) : kLado;
        lv_obj_set_pos(obj_, letra.x - kLado - 6, letra.y + (altura - kLado) / 2);
        lv_obj_remove_flag(obj_, LV_OBJ_FLAG_HIDDEN);
    }

    void Esconder() {
        if (timer_ != nullptr) {
            lv_timer_delete(timer_);
            timer_ = nullptr;
        }
        if (obj_ != nullptr) {
            lv_obj_delete(obj_);
            obj_ = nullptr;
        }
    }

    // O label vai ser apagado ou trocado: esquece o objeto sem apagar (já foi junto com o pai)
    void Soltar() {
        if (timer_ != nullptr) {
            lv_timer_delete(timer_);
            timer_ = nullptr;
        }
        obj_ = nullptr;
    }

    lv_obj_t* Objeto() const { return obj_; }
    void IrParaQuadro(int passo) { passo_ = passo; }  // simulador: fotografa um quadro

private:
    static constexpr uint32_t kQuadroMs = 120;
    static constexpr uint32_t kLaranja = 0xD97757;

    // · ✢ ✳ ✶ ✻ ✽: raios, comprimento e espessura (px); 0 raios = ponto
    struct Quadro {
        int raios;
        int comprimento;
        int espessura;
        int miolo;  // raios começam a esta distância do centro
    };
    static constexpr Quadro kQuadros[] = {
        {0, 0, 5, 0},    // ·
        {4, 7, 4, 0},    // ✢ (+)
        {8, 9, 3, 0},    // ✳
        {6, 10, 4, 0},   // ✶
        {8, 11, 4, 2},   // ✻
        {8, 12, 5, 3},   // ✽
    };
    static constexpr int kN = sizeof(kQuadros) / sizeof(kQuadros[0]);

    lv_obj_t* obj_ = nullptr;
    lv_timer_t* timer_ = nullptr;
    int passo_ = 0;  // 0..2*(kN-1)-1: vai de · a ✽ e volta

    static void Avancar(lv_timer_t* t) {
        auto self = static_cast<AsteriscoClaude*>(lv_timer_get_user_data(t));
        self->passo_ = (self->passo_ + 1) % (2 * (kN - 1));
        if (self->obj_ != nullptr) {
            lv_obj_invalidate(self->obj_);
        }
    }

    static void Desenhar(lv_event_t* e) {
        auto self = static_cast<AsteriscoClaude*>(lv_event_get_user_data(e));
        auto obj = static_cast<lv_obj_t*>(lv_event_get_target(e));
        lv_layer_t* layer = lv_event_get_layer(e);
        int i = self->passo_ < kN ? self->passo_ : 2 * (kN - 1) - self->passo_;
        const Quadro& q = kQuadros[i];
        lv_area_t area;
        lv_obj_get_coords(obj, &area);
        int cx = (area.x1 + area.x2) / 2, cy = (area.y1 + area.y2) / 2;

        lv_draw_line_dsc_t linha;
        lv_draw_line_dsc_init(&linha);
        linha.color = lv_color_hex(kLaranja);
        linha.width = q.espessura;
        linha.round_start = 1;
        linha.round_end = 1;
        if (q.raios == 0) {  // ponto
            linha.p1.x = linha.p2.x = cx;
            linha.p1.y = cy;
            linha.p2.y = cy + 1;
            lv_draw_line(layer, &linha);
            return;
        }
        int giro = self->passo_ * 15;  // gira um pouco a cada quadro
        for (int r = 0; r < q.raios; r++) {
            int ang = (giro + r * 360 / q.raios) % 360;
            int32_t s = lv_trigo_sin(ang), c = lv_trigo_cos(ang);
            linha.p1.x = cx + (c * q.miolo) / 32767;
            linha.p1.y = cy + (s * q.miolo) / 32767;
            linha.p2.x = cx + (c * q.comprimento) / 32767;
            linha.p2.y = cy + (s * q.comprimento) / 32767;
            lv_draw_line(layer, &linha);
        }
    }
};

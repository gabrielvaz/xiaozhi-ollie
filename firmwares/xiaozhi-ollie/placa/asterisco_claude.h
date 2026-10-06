// Asterisco animado do Claude Code, antes de "Codando…", "Refatorando…" na tela de espera: como o spinner
// do CLI, vai e volta por · ✢ ✳ ✶ ✻ ✽ (cresce, vira +, ganha pontas) a cada 120 ms, girando um pouco a
// cada quadro. As fontes do Watcher não têm esses glifos: cada quadro é desenhado em vetor (raios com
// pontas redondas). O texto pede o asterisco com "* " no começo; quem chama trava a tela.
#pragma once

#include <lvgl.h>

#include <cstring>
#include <string>

class AsteriscoClaude {
public:
    static constexpr const char* kMarca = "* ";  // começo do texto que pede o asterisco
    static constexpr int kLado = 26;
    static constexpr int kVao = 8;               // entre o asterisco e o verbo

    ~AsteriscoClaude() { Esconder(); }

    // Se o texto começa com a marca, guarda a 1ª linha ("Codando…") e deixa no lugar dela uma linha vazia:
    // o asterisco e o verbo vão juntos num container centrado de verdade (com espaços reservando o lugar,
    // o verbo ficava fora do centro, e mais ainda na JetBrains Mono, de espaço largo)
    bool PrepararTexto(std::string& texto) {
        if (texto.rfind(kMarca, 0) != 0) {
            return false;
        }
        size_t fim = texto.find('\n');
        verbo_ = texto.substr(2, fim == std::string::npos ? std::string::npos : fim - 2);
        texto = " " + (fim == std::string::npos ? std::string() : texto.substr(fim));
        return true;
    }

    // Mostra asterisco + verbo centrados na 1ª linha do label (rolam junto com ele)
    void Mostrar(lv_obj_t* label) {
        if (linha_ == nullptr || lv_obj_get_parent(linha_) != label) {
            Esconder();
            linha_ = lv_obj_create(label);
            lv_obj_remove_style_all(linha_);
            lv_obj_set_size(linha_, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_add_flag(linha_, LV_OBJ_FLAG_IGNORE_LAYOUT);
            lv_obj_remove_flag(linha_, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_flex_flow(linha_, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(linha_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            lv_obj_set_style_pad_column(linha_, kVao, 0);
            obj_ = lv_obj_create(linha_);
            lv_obj_remove_style_all(obj_);
            lv_obj_set_size(obj_, kLado, kLado);
            lv_obj_add_event_cb(obj_, Desenhar, LV_EVENT_DRAW_MAIN, this);
            rotulo_ = lv_label_create(linha_);
            timer_ = lv_timer_create(Avancar, kQuadroMs, this);
        }
        auto fonte = lv_obj_get_style_text_font(label, LV_PART_MAIN);
        lv_obj_set_style_text_font(rotulo_, fonte, 0);
        lv_obj_set_style_text_color(rotulo_, lv_obj_get_style_text_color(label, LV_PART_MAIN), 0);
        lv_label_set_text(rotulo_, verbo_.c_str());
        int altura = fonte != nullptr ? lv_font_get_line_height(fonte) : kLado;
        lv_obj_set_height(linha_, altura);
        lv_obj_align(linha_, LV_ALIGN_TOP_MID, 0, 0);
        lv_obj_remove_flag(linha_, LV_OBJ_FLAG_HIDDEN);
    }

    void Esconder() {
        if (timer_ != nullptr) {
            lv_timer_delete(timer_);
            timer_ = nullptr;
        }
        if (linha_ != nullptr) {
            lv_obj_delete(linha_);  // leva junto o asterisco e o verbo
            linha_ = nullptr;
        }
        obj_ = rotulo_ = nullptr;
    }

    // Tela apagada: para de animar (economia de bateria)
    void Pausar(bool pausar) {
        if (timer_ != nullptr) {
            pausar ? lv_timer_pause(timer_) : lv_timer_resume(timer_);
        }
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

    lv_obj_t* linha_ = nullptr;   // container: asterisco + verbo
    lv_obj_t* obj_ = nullptr;     // o asterisco desenhado
    lv_obj_t* rotulo_ = nullptr;  // o verbo
    std::string verbo_;
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

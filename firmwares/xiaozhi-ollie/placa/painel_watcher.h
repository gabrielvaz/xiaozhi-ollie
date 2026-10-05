// Telas por cima da interface do SenseCAP Watcher (tela redonda 412x412):
// gaveta de ações, lista de sessões, detalhe com botões e gravação de reunião.
// Desenha na camada de cima do LVGL; quem chama não precisa travar a tela.
#pragma once

#include <cstring>

#include <lvgl.h>

#include <algorithm>
#include <string>
#include <vector>

#include <functional>

#include "display.h"
#include "fontes_watcher.h"
#include "material_symbols.h"

// qrcodegen (Nayuki, MIT) já vem no componente esp_emote_gfx; ativar o lv_qrcode duplicaria os símbolos
extern "C" {
bool qrcodegen_encodeText(const char* text, uint8_t tempBuffer[], uint8_t qrcode[], int ecl, int minVersion,
                          int maxVersion, int mask, bool boostEcl);
int qrcodegen_getSize(const uint8_t qrcode[]);
bool qrcodegen_getModule(const uint8_t qrcode[], int x, int y);
}

LV_FONT_DECLARE(font_material_symbols_30_4);

class PainelWatcher {
public:
    struct Item {
        std::string titulo;
        std::string detalhe;
        const char* icone = nullptr;  // MATERIAL_SYMBOLS_* (opcional)
    };

    explicit PainelWatcher(Display* display) : display_(display) {}

    bool Aberto() const { return raiz_ != nullptr; }
    int Selecionado() const { return selecionado_; }

    // Lista navegável: mostra 3 itens por vez, o do meio é o selecionado
    void MostrarLista(const std::string& cabecalho, const std::vector<Item>& itens, int selecionado = 0) {
        DisplayLockGuard lock(display_);
        Recriar();
        modo_ = Modo::Lista;
        itens_ = itens;
        selecionado_ = std::clamp(selecionado, 0, std::max(0, (int)itens_.size() - 1));
        Cabecalho(cabecalho);
        DesenharLista();
    }

    // Mosaico de ícones: 3 por linha, 3 linhas visíveis (rola para seguir o selecionado);
    // o nome do item selecionado aparece embaixo da grade. A roda anda célula por célula.
    void MostrarGrade(const std::string& cabecalho, const std::vector<Item>& itens, int selecionado = 0) {
        DisplayLockGuard lock(display_);
        Recriar();
        modo_ = Modo::Grade;
        itens_ = itens;
        selecionado_ = std::clamp(selecionado, 0, std::max(0, (int)itens_.size() - 1));
        Cabecalho(cabecalho);
        DesenharGrade();
    }

    // Texto com botões embaixo (a roda troca o botão selecionado)
    void MostrarTexto(const std::string& cabecalho, const std::string& texto, const std::vector<std::string>& botoes) {
        DisplayLockGuard lock(display_);
        Recriar();
        modo_ = Modo::Texto;
        botoes_ = botoes;
        selecionado_ = 0;
        Cabecalho(cabecalho);
        // Caixa rolável: a roda desce o texto até o fim e só depois passa para os botões
        texto_box_ = lv_obj_create(raiz_);
        lv_obj_set_size(texto_box_, 300, botoes.empty() ? 230 : 186);
        lv_obj_set_style_bg_opa(texto_box_, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(texto_box_, 0, 0);
        lv_obj_set_style_pad_all(texto_box_, 0, 0);
        lv_obj_set_scroll_dir(texto_box_, LV_DIR_VER);
        lv_obj_set_scrollbar_mode(texto_box_, LV_SCROLLBAR_MODE_ACTIVE);
        lv_obj_set_style_bg_color(texto_box_, lv_color_hex(0xD97757), LV_PART_SCROLLBAR);
        lv_obj_align(texto_box_, LV_ALIGN_TOP_MID, 0, 96);
        auto rotulo = lv_label_create(texto_box_);
        lv_obj_set_width(rotulo, 288);
        lv_label_set_long_mode(rotulo, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_font(rotulo, Fontes::Pequena(), 0);
        lv_obj_set_style_text_color(rotulo, lv_color_hex(0xEDEDED), 0);
        lv_obj_set_style_text_align(rotulo, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_text(rotulo, texto.c_str());
        lv_obj_align(rotulo, LV_ALIGN_TOP_MID, 0, 0);
        DesenharBotoes();
    }

    // Valor grande (cronômetro, contagem) com legenda e botões; Atualizar só troca o valor
    void MostrarValor(const std::string& cabecalho, const std::string& valor, const std::string& legenda,
                      const std::vector<std::string>& botoes, int selecionado = 0) {
        DisplayLockGuard lock(display_);
        Recriar();
        modo_ = Modo::Texto;
        botoes_ = botoes;
        selecionado_ = std::clamp(selecionado, 0, std::max(0, (int)botoes.size() - 1));
        Cabecalho(cabecalho);
        valor_ = Rotulo(Fontes::Grande(), 0xEDEDED, valor);
        lv_obj_set_style_transform_scale(valor_, 640, 0);  // 2,5x
        lv_obj_set_style_transform_pivot_x(valor_, LV_PCT(50), 0);
        lv_obj_set_style_transform_pivot_y(valor_, LV_PCT(50), 0);
        lv_obj_align(valor_, LV_ALIGN_CENTER, 0, -36);
        legenda_ = Rotulo(Fontes::Pequena(), 0x9A9A9A, legenda);
        lv_obj_set_width(legenda_, 300);
        lv_obj_set_style_text_align(legenda_, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(legenda_, LV_ALIGN_CENTER, 0, 34);
        DesenharBotoes();
    }

    // Atualiza o valor grande sozinho a cada `ms` (ex.: cronômetro com milissegundos); para ao trocar de tela
    void AtualizarRapido(std::function<std::string()> gerar, uint32_t ms) {
        DisplayLockGuard lock(display_);
        PararRapido();
        gerar_ = std::move(gerar);
        rapido_ = lv_timer_create([](lv_timer_t* t) {
            auto self = static_cast<PainelWatcher*>(lv_timer_get_user_data(t));
            if (self->valor_ != nullptr && self->gerar_) {
                lv_label_set_text(self->valor_, self->gerar_().c_str());
            }
        }, ms, this);
    }

    void PararRapido() {
        if (rapido_ != nullptr) {
            lv_timer_delete(rapido_);
            rapido_ = nullptr;
        }
    }

    void AtualizarValor(const std::string& valor, const std::string& legenda = "") {
        DisplayLockGuard lock(display_);
        if (valor_ != nullptr) {
            lv_label_set_text(valor_, valor.c_str());
        }
        if (legenda_ != nullptr && !legenda.empty()) {
            lv_label_set_text(legenda_, legenda.c_str());
        }
    }

    // QR code centralizado (fundo branco para leitura) com título em cima e legenda embaixo
    void MostrarQr(const std::string& titulo, const std::string& conteudo, const std::string& legenda) {
        DisplayLockGuard lock(display_);
        Recriar();
        modo_ = Modo::Texto;
        botoes_.clear();
        Cabecalho(titulo);
        auto fundo = lv_obj_create(raiz_);
        lv_obj_set_size(fundo, 236, 236);
        lv_obj_set_style_bg_color(fundo, lv_color_white(), 0);
        lv_obj_set_style_radius(fundo, 16, 0);
        lv_obj_set_style_border_width(fundo, 0, 0);
        lv_obj_set_style_pad_all(fundo, 0, 0);
        lv_obj_remove_flag(fundo, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_align(fundo, LV_ALIGN_CENTER, 0, 6);
        // Gera o QR (até versão 10 = 57x57 módulos) e desenha num canvas com módulos inteiros
        static uint8_t qr[((10 * 4 + 17) * (10 * 4 + 17) + 7) / 8 + 1];
        static uint8_t temporario[sizeof(qr)];
        if (qrcodegen_encodeText(conteudo.c_str(), temporario, qr, 1 /* Ecc MEDIUM */, 1, 10, -1 /* máscara auto */, true)) {
            int n = qrcodegen_getSize(qr);
            int escala = std::max(1, 212 / (n + 4));  // borda de 2 módulos de cada lado
            int lado = (n + 4) * escala;
            if (qr_buf_ != nullptr) {
                lv_draw_buf_destroy(qr_buf_);
            }
            qr_buf_ = lv_draw_buf_create(lado, lado, LV_COLOR_FORMAT_RGB565, 0);
            auto canvas = lv_canvas_create(fundo);
            lv_canvas_set_draw_buf(canvas, qr_buf_);
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
            lv_obj_center(canvas);
        } else {
            auto erro = Rotulo(Fontes::Pequena(), 0x000000, "Texto longo demais para QR", fundo);
            lv_obj_center(erro);
        }
        auto r = Rotulo(Fontes::Pequena(), 0x9A9A9A, legenda);
        lv_obj_set_width(r, 260);
        lv_label_set_long_mode(r, LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_align(r, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(r, LV_ALIGN_CENTER, 0, 142);
    }

    enum class Status { Carregando, Sucesso, Erro };
    // Ícone especial para Item::icone: sol desenhado em vetor (usado pelo app de previsão do tempo)
    static constexpr const char* kIconeSol = "\x01sol";

    // Status com ícone: spinner girando (carregando), check verde (sucesso) ou X vermelho (erro)
    void MostrarStatus(const std::string& cabecalho, Status status, const std::string& texto,
                       const std::vector<std::string>& botoes = {}) {
        DisplayLockGuard lock(display_);
        Recriar();
        modo_ = Modo::Texto;
        botoes_ = botoes;
        selecionado_ = 0;
        Cabecalho(cabecalho);
        bool carregando = status == Status::Carregando;
        if (carregando) {
            auto arco = Spinner(raiz_, 110, 12, 0x2A2A2A);
            lv_obj_align(arco, LV_ALIGN_CENTER, 0, 0);  // no centro da tela, na altura também
        } else {
            // Check ou X desenhado em vetor (o ícone da fonte ampliado ficava borrado)
            bool ok = status == Status::Sucesso;
            auto circulo = lv_obj_create(raiz_);
            lv_obj_set_size(circulo, 110, 110);
            lv_obj_set_style_radius(circulo, LV_RADIUS_CIRCLE, 0);
            lv_obj_set_style_border_width(circulo, 0, 0);
            lv_obj_set_style_pad_all(circulo, 0, 0);
            lv_obj_set_style_bg_color(circulo, lv_color_hex(ok ? 0x3FB950 : 0xE5484D), 0);
            lv_obj_remove_flag(circulo, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_align(circulo, LV_ALIGN_CENTER, 0, 0);
            static const lv_point_precise_t kCheck[] = {{30, 57}, {48, 75}, {82, 38}};
            static const lv_point_precise_t kX1[] = {{36, 36}, {74, 74}};
            static const lv_point_precise_t kX2[] = {{74, 36}, {36, 74}};
            auto traco = [&](const lv_point_precise_t* pontos, int n) {
                auto l = lv_line_create(circulo);
                lv_line_set_points(l, pontos, n);
                lv_obj_set_style_line_width(l, 12, 0);
                lv_obj_set_style_line_rounded(l, true, 0);
                lv_obj_set_style_line_color(l, lv_color_hex(0x000000), 0);
                lv_obj_set_pos(l, 0, 0);
            };
            if (ok) {
                traco(kCheck, 3);
            } else {
                traco(kX1, 2);
                traco(kX2, 2);
            }
        }
        // Com botões, ícone e texto sobem um pouco e os botões descem, para não sobrepor a mensagem
        bool com_botoes = !botoes.empty();
        if (com_botoes) {
            lv_obj_set_y(lv_obj_get_child(raiz_, -1), -40);
        }
        auto r = Rotulo(Fontes::Pequena(), 0xEDEDED, texto);
        lv_obj_set_width(r, 290);
        lv_obj_set_style_text_align(r, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(r, LV_ALIGN_CENTER, 0, com_botoes ? 48 : 86);
        int y_antes = botoes_y_;
        if (com_botoes) {
            botoes_y_ = 316;
        }
        DesenharBotoes();
        botoes_y_ = y_antes;
    }

    struct DiaTempo {
        std::string nome, categoria;
        int min = 0, max = 0, chuva = 0;
    };

    // Previsão do tempo: ícone grande + temperatura, descrição, hoje/amanhã e botões
    void MostrarTempo(const std::string& local, int temp, const std::string& categoria, bool noite,
                      const std::string& descricao, int sensacao, int umidade, const std::vector<DiaTempo>& dias,
                      const std::vector<std::string>& botoes) {
        DisplayLockGuard lock(display_);
        Recriar();
        modo_ = Modo::Texto;
        botoes_ = botoes;
        selecionado_ = 0;
        Cabecalho(local);
        // Ícone (72 px) e temperatura lado a lado; descrição e detalhes logo abaixo, sem sobreposição
        IconeTempo(raiz_, categoria, noite, 72, -56, 90);
        auto t = Rotulo(Fontes::Grande(), 0xEDEDED, std::to_string(temp) + "°");
        lv_obj_set_style_transform_scale(t, 512, 0);  // 2x, crescendo a partir do canto de cima
        lv_obj_align(t, LV_ALIGN_TOP_MID, 30, 98);
        auto d = Rotulo(Fontes::Pequena(), 0xEDEDED, descricao);
        lv_obj_set_width(d, 300);
        lv_label_set_long_mode(d, LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_align(d, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(d, LV_ALIGN_TOP_MID, 0, 168);
        auto s = Rotulo(Fontes::Pequena(), 0x9A9A9A,
                        "sensação " + std::to_string(sensacao) + "° · umidade " + std::to_string(umidade) + "%");
        lv_obj_align(s, LV_ALIGN_TOP_MID, 0, 192);
        // Hoje e amanhã: cada linha centrada (ícone + texto), acima dos botões
        for (size_t i = 0; i < dias.size() && i < 2; i++) {
            int y = 222 + (int)i * 38;
            auto linha = lv_obj_create(raiz_);
            lv_obj_set_size(linha, LV_SIZE_CONTENT, 34);
            lv_obj_set_style_bg_opa(linha, LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_width(linha, 0, 0);
            lv_obj_set_style_pad_all(linha, 0, 0);
            lv_obj_set_style_pad_column(linha, 8, 0);
            lv_obj_remove_flag(linha, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_flex_flow(linha, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(linha, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            lv_obj_align(linha, LV_ALIGN_TOP_MID, 0, y);
            IconeTempo(linha, dias[i].categoria, false, 30, 0, 0);
            Rotulo(Fontes::Pequena(), 0xEDEDED,
                   dias[i].nome + "  " + std::to_string(dias[i].min) + "°/" + std::to_string(dias[i].max) +
                       "°  · chuva " + std::to_string(dias[i].chuva) + "%",
                   linha);
        }
        botoes_y_ = 314;  // a barra de botões cabe na parte mais larga do fim do círculo
        DesenharBotoes();
        botoes_y_ = 292;
    }

    // Uso do Claude: arco externo = janela de 5 h, arco interno = semana (cores por nível)
    void MostrarUso(int pct_5h, int reinicio_5h_s, int pct_7d, int reinicio_7d_s) {
        DisplayLockGuard lock(display_);
        Recriar();
        modo_ = Modo::Texto;
        botoes_.clear();
        auto arco = [&](int tamanho, int pct) {
            auto a = lv_arc_create(raiz_);
            lv_obj_set_size(a, tamanho, tamanho);
            lv_arc_set_rotation(a, 135);
            lv_arc_set_bg_angles(a, 0, 270);
            lv_arc_set_range(a, 0, 100);
            lv_arc_set_value(a, std::clamp(pct, 0, 100));
            lv_obj_remove_style(a, nullptr, LV_PART_KNOB);
            lv_obj_remove_flag(a, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_arc_width(a, 16, LV_PART_MAIN);
            lv_obj_set_style_arc_width(a, 16, LV_PART_INDICATOR);
            lv_obj_set_style_arc_color(a, lv_color_hex(0x2A2A2A), LV_PART_MAIN);
            lv_obj_set_style_arc_color(a, lv_color_hex(CorNivel(pct)), LV_PART_INDICATOR);
            lv_obj_center(a);
        };
        arco(370, pct_5h);
        arco(326, pct_7d);
        auto linha = [&](const lv_font_t* f, uint32_t cor, const std::string& t, int y) {
            auto r = Rotulo(f, cor, t);
            lv_obj_align(r, LV_ALIGN_CENTER, 0, y);
        };
        linha(Fontes::Grande(), 0xD97757, "Uso do Claude", -96);
        linha(Fontes::Grande(), CorNivel(pct_5h), "5 h: " + std::to_string(pct_5h) + "%", -44);
        linha(Fontes::Pequena(), 0x9A9A9A, "reinicia em " + Duracao(reinicio_5h_s), -14);
        linha(Fontes::Grande(), CorNivel(pct_7d), "Semana: " + std::to_string(pct_7d) + "%", 30);
        linha(Fontes::Pequena(), 0x9A9A9A, "reinicia em " + Duracao(reinicio_7d_s), 60);
        linha(Fontes::Pequena(), 0x6E6E6E, "Clique para voltar", 112);
    }

    // Gravação de reunião: bolinha vermelha pulsando (parada e cinza em pausa), tempo gravado e dois botões
    // escolhidos pela roda: 0 = Pausar/Continuar, 1 = Parar
    void MostrarGravacao(int segundos, bool pausado = false, int botao = 0) {
        DisplayLockGuard lock(display_);
        if (modo_ != Modo::Gravacao || raiz_ == nullptr) {
            Recriar();
            modo_ = Modo::Gravacao;
            ponto_ = lv_obj_create(raiz_);
            lv_obj_set_size(ponto_, 34, 34);
            lv_obj_set_style_radius(ponto_, LV_RADIUS_CIRCLE, 0);
            lv_obj_set_style_border_width(ponto_, 0, 0);
            lv_obj_remove_flag(ponto_, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_align(ponto_, LV_ALIGN_TOP_MID, 0, 96);
            titulo_gravacao_ = Rotulo(Fontes::Grande(), 0xEDEDED, "");
            lv_obj_align(titulo_gravacao_, LV_ALIGN_TOP_MID, 0, 140);
            cronometro_ = Rotulo(Fontes::Grande(), 0xD97757, "00:00");
            lv_obj_set_style_transform_scale(cronometro_, 512, 0);  // 2x
            lv_obj_set_style_transform_pivot_x(cronometro_, LV_PCT(50), 0);
            lv_obj_align(cronometro_, LV_ALIGN_TOP_MID, 0, 200);
            for (int i = 0; i < 2; i++) {
                botoes_gravacao_[i] = lv_obj_create(raiz_);
                lv_obj_set_size(botoes_gravacao_[i], 124, 48);
                lv_obj_set_style_radius(botoes_gravacao_[i], 24, 0);
                lv_obj_set_style_border_width(botoes_gravacao_[i], 0, 0);
                lv_obj_set_style_pad_all(botoes_gravacao_[i], 0, 0);
                lv_obj_remove_flag(botoes_gravacao_[i], LV_OBJ_FLAG_SCROLLABLE);
                lv_obj_align(botoes_gravacao_[i], LV_ALIGN_TOP_MID, i == 0 ? -68 : 68, 288);
                auto r = Rotulo(Fontes::Pequena(), 0xEDEDED, i == 0 ? "Pausar" : "Parar", botoes_gravacao_[i]);
                lv_obj_center(r);
            }
            pausado_desenhado_ = !pausado;  // força o primeiro desenho do estado
        }
        if (pausado != pausado_desenhado_) {
            pausado_desenhado_ = pausado;
            lv_anim_delete(ponto_, nullptr);
            lv_label_set_text(titulo_gravacao_, pausado ? "Reunião pausada" : "Gravando reunião");
            lv_label_set_text(lv_obj_get_child(botoes_gravacao_[0], 0), pausado ? "Continuar" : "Pausar");
            lv_obj_set_style_bg_color(ponto_, lv_color_hex(pausado ? 0x6E6E6E : 0xE5484D), 0);
            lv_obj_set_size(ponto_, 34, 34);
            lv_obj_align(ponto_, LV_ALIGN_TOP_MID, 0, 96);
            if (!pausado) {  // pulsa: diminui e aumenta, centrado no mesmo ponto
                lv_anim_t a;
                lv_anim_init(&a);
                lv_anim_set_var(&a, ponto_);
                lv_anim_set_exec_cb(&a, [](void* obj, int32_t v) {
                    auto o = static_cast<lv_obj_t*>(obj);
                    lv_obj_set_size(o, v, v);
                    lv_obj_align(o, LV_ALIGN_TOP_MID, 0, 96 + (34 - v) / 2);
                });
                lv_anim_set_values(&a, 34, 20);
                lv_anim_set_duration(&a, 700);
                lv_anim_set_reverse_duration(&a, 700);
                lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
                lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
                lv_anim_start(&a);
            }
        }
        for (int i = 0; i < 2; i++) {
            bool sel = i == botao;
            lv_obj_set_style_bg_color(botoes_gravacao_[i], lv_color_hex(sel ? 0xD97757 : 0x2A2A2A), 0);
            lv_obj_set_style_text_color(lv_obj_get_child(botoes_gravacao_[i], 0), lv_color_hex(sel ? 0x000000 : 0xEDEDED), 0);
        }
        char tempo[32];
        if (segundos >= 3600) {
            snprintf(tempo, sizeof(tempo), "%d:%02d:%02d", segundos / 3600, (segundos / 60) % 60, segundos % 60);
        } else {
            snprintf(tempo, sizeof(tempo), "%02d:%02d", segundos / 60, segundos % 60);
        }
        lv_label_set_text(cronometro_, tempo);
    }

    void Mover(int delta) {
        DisplayLockGuard lock(display_);
        if (modo_ == Modo::Lista && !itens_.empty()) {
            selecionado_ = std::clamp(selecionado_ + delta, 0, (int)itens_.size() - 1);
            DesenharLista();
        } else if (modo_ == Modo::Grade && !itens_.empty()) {
            // Linear (esquerda→direita, linha a linha), travando nas pontas como a lista
            selecionado_ = std::clamp(selecionado_ + delta, 0, (int)itens_.size() - 1);
            AtualizarGrade(true);
        } else if (modo_ == Modo::Texto) {
            if (texto_box_ != nullptr) {
                lv_obj_update_layout(texto_box_);
                if (delta > 0 && lv_obj_get_scroll_bottom(texto_box_) > 0) {
                    lv_obj_scroll_by_bounded(texto_box_, 0, -72, LV_ANIM_ON);
                    return;
                }
                if (delta < 0 && selecionado_ == 0 && lv_obj_get_scroll_top(texto_box_) > 0) {
                    lv_obj_scroll_by_bounded(texto_box_, 0, 72, LV_ANIM_ON);
                    return;
                }
            }
            if (!botoes_.empty()) {
                selecionado_ = std::clamp(selecionado_ + delta, 0, (int)botoes_.size() - 1);
                DesenharBotoes();
            }
        }
    }

    // Depois de MostrarTexto: começa mostrando o fim (mensagens mais recentes); a roda sobe e desce
    void RolarTextoParaFim() {
        DisplayLockGuard lock(display_);
        if (modo_ == Modo::Texto && texto_box_ != nullptr) {
            lv_obj_update_layout(texto_box_);
            lv_obj_scroll_to_y(texto_box_, LV_COORD_MAX, LV_ANIM_OFF);
        }
    }

    void Fechar() {
        DisplayLockGuard lock(display_);
        PararRapido();
        if (raiz_ != nullptr) {
            lv_obj_delete(raiz_);
        }
        LiberarQr();
        raiz_ = nullptr;
        texto_box_ = nullptr;
        conteudo_ = nullptr;
        barra_botoes_ = nullptr;
        cronometro_ = nullptr;
        ponto_ = nullptr;
        titulo_gravacao_ = nullptr;
        botoes_gravacao_[0] = botoes_gravacao_[1] = nullptr;
        valor_ = nullptr;
        legenda_ = nullptr;
        grade_ = nullptr;
        nome_grade_ = nullptr;
        celulas_.clear();
        modo_ = Modo::Nenhum;
    }

private:
    enum class Modo { Nenhum, Lista, Grade, Texto, Gravacao };

    // Medidas do mosaico (tela redonda 412x412): 3 colunas x 3 linhas visíveis de 78 px com 12 px
    // de espaço = 258x258, de y=90 a y=348; os cantos arredondados ficam dentro do círculo.
    static constexpr int kCelula = 78;
    static constexpr int kEspaco = 12;
    static constexpr int kColunas = 3;
    static constexpr int kLinhasVisiveis = 3;
    static constexpr int kGradeY = 90;

    Display* display_;
    lv_obj_t* raiz_ = nullptr;
    lv_obj_t* conteudo_ = nullptr;
    lv_obj_t* barra_botoes_ = nullptr;
    lv_obj_t* cronometro_ = nullptr;
    lv_obj_t* ponto_ = nullptr;             // gravação: bolinha vermelha
    lv_obj_t* titulo_gravacao_ = nullptr;
    lv_obj_t* botoes_gravacao_[2] = {nullptr, nullptr};
    bool pausado_desenhado_ = false;
    lv_obj_t* valor_ = nullptr;
    lv_obj_t* legenda_ = nullptr;
    lv_draw_buf_t* qr_buf_ = nullptr;  // imagem do QR (liberada ao trocar de tela)
    lv_obj_t* texto_box_ = nullptr;
    int botoes_y_ = 292;
    lv_timer_t* rapido_ = nullptr;
    std::function<std::string()> gerar_;
    Modo modo_ = Modo::Nenhum;
    std::vector<Item> itens_;
    lv_obj_t* grade_ = nullptr;       // contêiner rolável do mosaico
    lv_obj_t* nome_grade_ = nullptr;  // nome do item selecionado (embaixo da grade)
    std::vector<lv_obj_t*> celulas_;  // uma por item, na ordem de itens_
    std::vector<std::string> botoes_;
    int selecionado_ = 0;

    // ------------------------------------------------------------ ícones de tempo (formas LVGL)

    lv_obj_t* Forma(lv_obj_t* pai, int w, int h, int x, int y, uint32_t cor, int raio = LV_RADIUS_CIRCLE) {
        auto o = lv_obj_create(pai);
        lv_obj_set_size(o, w, h);
        lv_obj_set_style_radius(o, raio, 0);
        lv_obj_set_style_bg_color(o, lv_color_hex(cor), 0);
        lv_obj_set_style_border_width(o, 0, 0);
        lv_obj_set_style_pad_all(o, 0, 0);
        lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_pos(o, x, y);
        return o;
    }

    void Nuvem(lv_obj_t* c, int s, int dy, uint32_t cor) {
        Forma(c, s * 34 / 100, s * 34 / 100, s * 14 / 100, dy + s * 24 / 100, cor);
        Forma(c, s * 46 / 100, s * 46 / 100, s * 32 / 100, dy + s * 10 / 100, cor);
        Forma(c, s * 80 / 100, s * 26 / 100, s * 10 / 100, dy + s * 34 / 100, cor, s / 8);
    }

    // categoria: sol, parcial, nuvem, neblina, garoa, chuva, neve, tempestade. (dx, y) = centro x relativo e topo
    void IconeTempo(lv_obj_t* pai, const std::string& cat, bool noite, int s, int dx, int y) {
        auto c = lv_obj_create(pai);
        lv_obj_set_size(c, s, s);
        lv_obj_set_style_bg_opa(c, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(c, 0, 0);
        lv_obj_set_style_pad_all(c, 0, 0);
        lv_obj_remove_flag(c, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_align(c, LV_ALIGN_TOP_MID, dx, y);
        const uint32_t amarelo = 0xF5C542, cinza = 0xB8BCC4, escuro = 0x6B7280, azul = 0x4FA3F7;
        auto astro = [&](int d, int x, int yy) {
            if (noite) {
                Forma(c, d, d, x, yy, 0xE5E7EB);
                Forma(c, d * 3 / 4, d * 3 / 4, x + d / 3, yy - d / 10, 0x000000);  // lua crescente
            } else {
                Forma(c, d, d, x, yy, amarelo);
            }
        };
        if (cat == "sol") {
            astro(s * 64 / 100, s * 18 / 100, s * 18 / 100);
        } else if (cat == "parcial") {
            astro(s * 46 / 100, s * 44 / 100, s * 8 / 100);
            Nuvem(c, s, s * 12 / 100, cinza);
        } else if (cat == "neblina") {
            for (int i = 0; i < 3; i++) {
                Forma(c, s * (70 - i * 10) / 100, s / 12, s * (14 + i * 6) / 100, s * (30 + i * 18) / 100, cinza, s / 24);
            }
        } else {
            bool pesada = cat == "tempestade" || cat == "chuva";
            Nuvem(c, s, 0, pesada ? escuro : cinza);
            if (cat == "garoa" || cat == "chuva" || cat == "tempestade") {
                int gotas = cat == "garoa" ? 2 : 3;
                for (int i = 0; i < gotas; i++) {
                    Forma(c, s / 14, s * 18 / 100, s * (28 + i * 18) / 100, s * 66 / 100, azul, s / 28);
                }
            }
            if (cat == "neve") {
                for (int i = 0; i < 3; i++) {
                    Forma(c, s / 10, s / 10, s * (28 + i * 18) / 100, s * 70 / 100, 0xFFFFFF);
                }
            }
            if (cat == "tempestade") {  // raio
                auto r = Forma(c, s / 10, s * 30 / 100, s * 60 / 100, s * 58 / 100, amarelo, 2);
                lv_obj_set_style_transform_rotation(r, 200, 0);
            }
        }
    }

    static uint32_t CorNivel(int pct) {
        return pct >= 90 ? 0xE5484D : (pct >= 70 ? 0xF5A524 : 0x3FB950);
    }

    static std::string Duracao(int s) {
        if (s < 0) {
            return "?";
        }
        if (s >= 86400) {
            return std::to_string(s / 86400) + " d " + std::to_string(s % 86400 / 3600) + " h";
        }
        if (s >= 3600) {
            return std::to_string(s / 3600) + " h " + std::to_string(s % 3600 / 60) + " min";
        }
        return std::to_string(s / 60) + " min";
    }

    // Spinner (arco laranja girando) reaproveitado nas telas de carregamento e nas listas
    static lv_obj_t* Spinner(lv_obj_t* pai, int tamanho, int espessura, uint32_t cor_fundo,
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
        lv_anim_start(&a);
        return arco;
    }

    lv_obj_t* Rotulo(const lv_font_t* fonte, uint32_t cor, const std::string& texto, lv_obj_t* pai = nullptr) {
        auto r = lv_label_create(pai ? pai : raiz_);
        lv_obj_set_style_text_font(r, fonte, 0);
        lv_obj_set_style_text_color(r, lv_color_hex(cor), 0);
        lv_label_set_text(r, texto.c_str());
        return r;
    }

    void LiberarQr() {
        if (qr_buf_ != nullptr) {
            lv_draw_buf_destroy(qr_buf_);
            qr_buf_ = nullptr;
        }
    }

    void Recriar() {
        PararRapido();
        if (raiz_ != nullptr) {
            lv_obj_delete(raiz_);
        }
        LiberarQr();
        texto_box_ = nullptr;
        grade_ = nullptr;
        nome_grade_ = nullptr;
        celulas_.clear();
        raiz_ = lv_obj_create(lv_layer_top());
        lv_obj_set_size(raiz_, LV_HOR_RES, LV_VER_RES);
        lv_obj_set_style_bg_color(raiz_, lv_color_hex(0x000000), 0);
        lv_obj_set_style_bg_opa(raiz_, LV_OPA_90, 0);
        lv_obj_set_style_border_width(raiz_, 0, 0);
        lv_obj_set_style_radius(raiz_, 0, 0);
        lv_obj_set_style_pad_all(raiz_, 0, 0);
        lv_obj_set_scrollbar_mode(raiz_, LV_SCROLLBAR_MODE_OFF);
        lv_obj_remove_flag(raiz_, LV_OBJ_FLAG_SCROLLABLE);
        conteudo_ = nullptr;
        barra_botoes_ = nullptr;
        cronometro_ = nullptr;
        valor_ = nullptr;
        legenda_ = nullptr;
    }

    void Cabecalho(const std::string& texto) {
        auto r = Rotulo(Fontes::Grande(), 0xD97757, texto);
        lv_obj_set_width(r, 260);
        lv_label_set_long_mode(r, LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_align(r, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(r, LV_ALIGN_TOP_MID, 0, 50);
    }

    void DesenharLista() {
        if (conteudo_ != nullptr) {
            lv_obj_delete(conteudo_);
        }
        conteudo_ = lv_obj_create(raiz_);
        lv_obj_set_size(conteudo_, 330, 250);
        lv_obj_set_style_bg_opa(conteudo_, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(conteudo_, 0, 0);
        lv_obj_set_style_pad_all(conteudo_, 0, 0);
        lv_obj_remove_flag(conteudo_, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_align(conteudo_, LV_ALIGN_TOP_MID, 0, 92);
        if (itens_.empty()) {
            auto r = Rotulo(Fontes::Pequena(), 0x9A9A9A, "Nada por aqui", conteudo_);
            lv_obj_align(r, LV_ALIGN_CENTER, 0, 0);
            return;
        }
        for (int linha = 0; linha < 3; linha++) {
            int i = selecionado_ - 1 + linha;
            if (i < 0 || i >= (int)itens_.size()) {
                continue;
            }
            bool atual = i == selecionado_;
            auto caixa = lv_obj_create(conteudo_);
            lv_obj_set_size(caixa, atual ? 320 : 290, 72);
            lv_obj_set_style_radius(caixa, 18, 0);
            lv_obj_set_style_border_width(caixa, 0, 0);
            lv_obj_set_style_pad_all(caixa, 6, 0);
            lv_obj_remove_flag(caixa, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_bg_color(caixa, lv_color_hex(atual ? 0xD97757 : 0x1C1C1C), 0);
            lv_obj_set_style_bg_opa(caixa, atual ? LV_OPA_COVER : LV_OPA_70, 0);
            lv_obj_align(caixa, LV_ALIGN_TOP_MID, 0, linha * 84);
            const auto& item = itens_[i];
            uint32_t cor_texto = atual ? 0x000000 : 0xEDEDED;
            int largura = atual ? 300 : 270;
            int x_texto = 0;
            lv_text_align_t alinhamento = LV_TEXT_ALIGN_CENTER;
            if (item.icone != nullptr && strcmp(item.icone, MATERIAL_SYMBOLS_PROGRESS_ACTIVITY) == 0) {
                // Em execução: spinner girando no lugar do ícone
                auto sp = Spinner(caixa, 30, 4, atual ? 0xB85F42 : 0x2A2A2A, atual ? 0x000000 : 0xD97757);
                lv_obj_align(sp, LV_ALIGN_LEFT_MID, 8, 0);
                largura -= 50;
                x_texto = 50;
            } else if (item.icone != nullptr) {
                auto ic = Rotulo(&font_material_symbols_30_4, atual ? 0x000000 : 0xD97757, item.icone, caixa);
                lv_obj_align(ic, LV_ALIGN_LEFT_MID, 8, 0);
                largura -= 50;
                x_texto = 50;
                alinhamento = LV_TEXT_ALIGN_LEFT;
            }
            // Título e detalhe sempre em uma linha (altura fixa + reticências)
            auto t = Rotulo(Fontes::Grande(), cor_texto, item.titulo, caixa);
            lv_obj_set_size(t, largura, 30);
            lv_label_set_long_mode(t, LV_LABEL_LONG_DOT);
            lv_obj_set_style_text_align(t, alinhamento, 0);
            lv_obj_align(t, LV_ALIGN_TOP_LEFT, x_texto, item.detalhe.empty() ? 14 : 0);
            if (!item.detalhe.empty()) {
                auto d = Rotulo(Fontes::Pequena(), atual ? 0x2A140C : 0x9A9A9A, item.detalhe, caixa);
                lv_obj_set_size(d, largura, 26);
                lv_label_set_long_mode(d, LV_LABEL_LONG_DOT);
                lv_obj_set_style_text_align(d, alinhamento, 0);
                lv_obj_align(d, LV_ALIGN_TOP_LEFT, x_texto, 32);
            }
        }
        char posicao[32];
        snprintf(posicao, sizeof(posicao), "%d/%d", selecionado_ + 1, (int)itens_.size());
        auto p = Rotulo(Fontes::Pequena(), 0x6E6E6E, posicao, conteudo_);
        lv_obj_align(p, LV_ALIGN_BOTTOM_MID, 0, 0);
    }

    // Cria o mosaico uma vez; trocar a seleção só repinta as células (AtualizarGrade) e rola a grade
    void DesenharGrade() {
        if (itens_.empty()) {
            auto r = Rotulo(Fontes::Pequena(), 0x9A9A9A, "Nada por aqui");
            lv_obj_align(r, LV_ALIGN_CENTER, 0, 0);
            return;
        }
        const int largura = kColunas * kCelula + (kColunas - 1) * kEspaco;
        const int altura = kLinhasVisiveis * kCelula + (kLinhasVisiveis - 1) * kEspaco;
        grade_ = lv_obj_create(raiz_);
        lv_obj_set_size(grade_, largura, altura);
        lv_obj_set_style_bg_opa(grade_, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(grade_, 0, 0);
        lv_obj_set_style_radius(grade_, 0, 0);
        lv_obj_set_style_pad_all(grade_, 0, 0);
        lv_obj_set_style_pad_column(grade_, kEspaco, 0);
        lv_obj_set_style_pad_row(grade_, kEspaco, 0);
        lv_obj_set_flex_flow(grade_, LV_FLEX_FLOW_ROW_WRAP);
        lv_obj_set_flex_align(grade_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        lv_obj_set_scroll_dir(grade_, LV_DIR_VER);
        lv_obj_set_scrollbar_mode(grade_, LV_SCROLLBAR_MODE_OFF);
        lv_obj_remove_flag(grade_, LV_OBJ_FLAG_SCROLL_ELASTIC);
        lv_obj_align(grade_, LV_ALIGN_TOP_MID, 0, kGradeY);
        celulas_.clear();
        for (const auto& item : itens_) {
            auto celula = lv_obj_create(grade_);
            lv_obj_set_size(celula, kCelula, kCelula);
            lv_obj_set_style_radius(celula, 22, 0);
            lv_obj_set_style_border_width(celula, 0, 0);
            lv_obj_set_style_pad_all(celula, 0, 0);
            lv_obj_remove_flag(celula, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_remove_flag(celula, LV_OBJ_FLAG_CLICKABLE);
            // Ícone (filho 0 da célula) ampliado ~1,4x a partir do centro
            if (item.icone != nullptr && strcmp(item.icone, kIconeSol) == 0) {
                // Sol desenhado em vetor (a fonte de ícones não tem sol)
                IconeTempo(celula, "sol", false, 50, 0, (kCelula - 50) / 2);
                celulas_.push_back(celula);
                continue;
            }
            auto ic = Rotulo(&font_material_symbols_30_4, 0xD97757,
                             item.icone != nullptr ? item.icone : MATERIAL_SYMBOLS_ROBOT_2, celula);
            lv_obj_set_style_transform_scale(ic, 358, 0);
            lv_obj_set_style_transform_pivot_x(ic, LV_PCT(50), 0);
            lv_obj_set_style_transform_pivot_y(ic, LV_PCT(50), 0);
            lv_obj_center(ic);
            celulas_.push_back(celula);
        }
        // Nome do selecionado numa linha só, embaixo da grade (largura cabe na curva do círculo)
        nome_grade_ = Rotulo(Fontes::Grande(), 0xEDEDED, "");
        lv_obj_set_size(nome_grade_, 200, 30);
        lv_label_set_long_mode(nome_grade_, LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_align(nome_grade_, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(nome_grade_, LV_ALIGN_TOP_MID, 0, kGradeY + altura + 8);
        lv_obj_update_layout(grade_);
        AtualizarGrade(false);
    }

    void AtualizarGrade(bool animar) {
        if (grade_ == nullptr || celulas_.empty()) {
            return;
        }
        for (int i = 0; i < (int)celulas_.size(); i++) {
            bool atual = i == selecionado_;
            auto celula = celulas_[i];
            lv_obj_set_style_bg_color(celula, lv_color_hex(atual ? 0xD97757 : 0x1C1C1C), 0);
            lv_obj_set_style_bg_opa(celula, atual ? LV_OPA_COVER : LV_OPA_70, 0);
            auto ic = lv_obj_get_child(celula, 0);
            if (ic != nullptr) {
                lv_obj_set_style_text_color(ic, lv_color_hex(atual ? 0x000000 : 0xD97757), 0);
            }
        }
        if (nome_grade_ != nullptr && selecionado_ >= 0 && selecionado_ < (int)itens_.size()) {
            lv_label_set_text(nome_grade_, itens_[selecionado_].titulo.c_str());
        }
        // Rola o mínimo para a célula ficar visível; como a altura é múltipla da linha, para alinhada
        lv_obj_scroll_to_view(celulas_[selecionado_], animar ? LV_ANIM_ON : LV_ANIM_OFF);
    }

    void DesenharBotoes() {
        if (barra_botoes_ != nullptr) {
            lv_obj_delete(barra_botoes_);
            barra_botoes_ = nullptr;
        }
        if (botoes_.empty()) {
            return;
        }
        barra_botoes_ = lv_obj_create(raiz_);
        lv_obj_set_size(barra_botoes_, 320, 56);
        lv_obj_set_style_bg_opa(barra_botoes_, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(barra_botoes_, 0, 0);
        lv_obj_set_style_pad_all(barra_botoes_, 0, 0);
        lv_obj_remove_flag(barra_botoes_, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_flex_flow(barra_botoes_, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(barra_botoes_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(barra_botoes_, 12, 0);
        lv_obj_align(barra_botoes_, LV_ALIGN_TOP_MID, 0, botoes_y_);
        for (int i = 0; i < (int)botoes_.size(); i++) {
            bool atual = i == selecionado_;
            auto b = lv_obj_create(barra_botoes_);
            lv_obj_set_size(b, LV_SIZE_CONTENT, 48);
            lv_obj_set_style_radius(b, 24, 0);
            lv_obj_set_style_pad_hor(b, 16, 0);
            lv_obj_set_style_pad_ver(b, 0, 0);
            lv_obj_set_style_border_width(b, atual ? 0 : 2, 0);
            lv_obj_set_style_border_color(b, lv_color_hex(0x3A3A3A), 0);
            lv_obj_set_style_bg_color(b, lv_color_hex(atual ? 0xD97757 : 0x000000), 0);
            lv_obj_remove_flag(b, LV_OBJ_FLAG_SCROLLABLE);
            auto r = Rotulo(Fontes::Pequena(), atual ? 0x000000 : 0xEDEDED, botoes_[i], b);
            lv_obj_center(r);
        }
    }
};

// App "Sessões do Claude Code": lista as sessões do herdr, mostra a última mensagem
// e manda um pedido por voz para a sessão escolhida.
#pragma once

#include <atomic>
#include <vector>

#include "../nucleo_apps.h"

class AppSessoes : public AppWatcher {
public:
    const char* Nome() const override { return "Sessões"; }
    const char* Icone() const override { return MATERIAL_SYMBOLS_ROBOT_2; }
    std::string Detalhe() const override { return "Ver, ler e mandar pedidos"; }

    void Abrir(ContextoApps& c) override {
        tela_ = Tela::Carregando;
        c.painel.MostrarStatus("Sessões", PainelWatcher::Status::Carregando, "Carregando sessões…");
        buscar_ = true;
    }

    void Girar(ContextoApps& c, int passo) override {
        if (tela_ == Tela::Lista || tela_ == Tela::Detalhe) {
            c.painel.Mover(passo);
        }
    }

    bool Clicar(ContextoApps& c) override {
        int i = c.painel.Selecionado();
        switch (tela_) {
            case Tela::Carregando:
                return true;
            case Tela::Erro:
                return false;
            case Tela::Lista:
                if (i == 1) {  // resumo falado de todas as sessões ativas
                    c.Perguntar("Me dá uma atualização de todas as sessões ativas do Claude Code e do herdr: quantas estão "
                                "trabalhando, quais esperam aprovação, quais têm subagentes e o que cada uma concluiu ou está fazendo.");
                    return true;
                }
                if (i <= 0 || i > (int)sessoes_.size() + 1) {  // item 0 = Voltar
                    return false;
                }
                atual_ = i - 2;
                tela_ = Tela::Carregando;
                c.painel.MostrarStatus(sessoes_[atual_].titulo, PainelWatcher::Status::Carregando, "Lendo as mensagens…");
                ler_mensagens_ = true;
                return true;
            case Tela::Detalhe:
                if (i == 0) {
                    const auto& s = sessoes_[atual_];
                    c.Perguntar("Quero mandar um pedido para a sessão \"" + s.titulo + "\" (id " + s.id +
                                "). Pergunte o que devo enviar e, depois que eu responder, confirme e use sessao_instruir nessa sessão.");
                } else {
                    MostrarLista(c, atual_);
                }
                return true;
        }
        return false;
    }

    bool Voltar(ContextoApps& c) override {
        if (tela_ == Tela::Detalhe) {
            MostrarLista(c, atual_);
            return true;
        }
        return false;
    }

    void Tique(ContextoApps& c) override {
        if (ler_mensagens_.exchange(false)) {
            LerMensagens(c);
            return;
        }
        if (!buscar_.exchange(false)) {
            return;
        }
        std::string corpo;
        if (!RedeWatcher::Pedir("GET", "/watcher/sessoes", "", corpo)) {
            tela_ = Tela::Erro;
            c.painel.MostrarTexto("Sessões", "Não consegui falar com o Mac agora.", {"Voltar"});
            return;
        }
        sessoes_.clear();
        cJSON* raiz = cJSON_Parse(corpo.c_str());
        cJSON* lista = raiz ? cJSON_GetObjectItem(raiz, "sessoes") : nullptr;
        cJSON* item = nullptr;
        cJSON_ArrayForEach(item, lista) {
            sessoes_.push_back({RedeWatcher::Campo(item, "id"), RedeWatcher::Campo(item, "titulo"),
                                RedeWatcher::Campo(item, "situacao"), RedeWatcher::Campo(item, "ha"),
                                RedeWatcher::Campo(item, "ultima")});
        }
        cJSON_Delete(raiz);
        MostrarLista(c, 0);
    }

    static const char* IconeSituacao(const std::string& s) {
        if (s == "Esperando você") return MATERIAL_SYMBOLS_WARNING;
        if (s == "Trabalhando") return MATERIAL_SYMBOLS_PROGRESS_ACTIVITY;
        if (s == "Subagentes") return MATERIAL_SYMBOLS_PROGRESS_ACTIVITY;  // em execução: spinner
        if (s == "Concluída") return MATERIAL_SYMBOLS_CHECK_CIRCLE;
        return MATERIAL_SYMBOLS_SCHEDULE;
    }

private:
    enum class Tela { Carregando, Erro, Lista, Detalhe };
    struct Sessao {
        std::string id, titulo, situacao, ha, ultima;
    };
    std::atomic<Tela> tela_{Tela::Carregando};
    std::atomic<bool> buscar_{false};
    std::atomic<bool> ler_mensagens_{false};
    std::string mensagens_;  // últimas mensagens da sessão aberta
    std::vector<Sessao> sessoes_;
    int atual_ = 0;

    void MostrarLista(ContextoApps& c, int selecionar) {
        tela_ = Tela::Lista;
        std::vector<PainelWatcher::Item> itens = {{"Voltar", "", MATERIAL_SYMBOLS_ARROW_BACK},
                                                  {"Resumir todas", "O Ollie fala o estado de cada sessão",
                                                   MATERIAL_SYMBOLS_HEADPHONES}};
        for (const auto& s : sessoes_) {
            itens.push_back({s.titulo, s.situacao + (s.ha.empty() ? "" : " · há " + s.ha), IconeSituacao(s.situacao)});
        }
        c.painel.MostrarLista("Sessões", itens, selecionar + 2);
    }

    static std::string Codificar(const std::string& texto) {
        static const char* hex = "0123456789ABCDEF";
        std::string saida;
        for (unsigned char ch : texto) {
            if (isalnum(ch) || ch == '-' || ch == '_' || ch == '.' || ch == '~') {
                saida += (char)ch;
            } else {
                saida += '%';
                saida += hex[ch >> 4];
                saida += hex[ch & 15];
            }
        }
        return saida;
    }

    // Últimas mensagens da sessão (Você / Claude), da mais antiga para a mais recente
    void LerMensagens(ContextoApps& c) {
        const auto& s = sessoes_[atual_];
        mensagens_.clear();
        std::string corpo;
        if (RedeWatcher::Pedir("GET", "/watcher/sessoes/" + Codificar(s.id) + "/mensagens", "", corpo)) {
            cJSON* raiz = cJSON_Parse(corpo.c_str());
            cJSON* lista = raiz ? cJSON_GetObjectItem(raiz, "mensagens") : nullptr;
            cJSON* m = nullptr;
            cJSON_ArrayForEach(m, lista) {
                std::string hora = RedeWatcher::Campo(m, "hora");
                mensagens_ += (mensagens_.empty() ? "" : "\n\n") + RedeWatcher::Campo(m, "quem") +
                              (hora.empty() ? "" : " · " + hora) + "\n" + RedeWatcher::Campo(m, "texto");
            }
            cJSON_Delete(raiz);
        }
        if (mensagens_.empty()) {
            mensagens_ = s.ultima.empty() ? "Sem mensagem registrada." : s.ultima;
        }
        MostrarDetalhe(c);
    }

    void MostrarDetalhe(ContextoApps& c) {
        tela_ = Tela::Detalhe;
        const auto& s = sessoes_[atual_];
        c.painel.MostrarTexto(s.titulo, s.situacao + (s.ha.empty() ? "" : " · há " + s.ha) + "\n\n" + mensagens_,
                              {"Enviar pedido", "Voltar"});
        c.painel.RolarTextoParaFim();
    }
};

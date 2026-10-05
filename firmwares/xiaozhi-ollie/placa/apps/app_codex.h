// App "Codex": controla o Codex do Mac de longe. Mostra o controle remoto (ligado/desligado),
// as sessões do Codex (últimas mensagens e enviar mensagem por voz), as tarefas do Codex Cloud
// e cria uma tarefa nova por voz. Rede no Tique (fora da trava da gaveta).
#pragma once

#include <atomic>
#include <cctype>
#include <vector>

#include "../nucleo_apps.h"

class AppCodex : public AppWatcher {
public:
    const char* Nome() const override { return "Codex"; }
    const char* Icone() const override { return MATERIAL_SYMBOLS_KEYBOARD_DOUBLE_ARROW_RIGHT; }
    std::string Detalhe() const override { return "Sessões, nuvem e controle remoto"; }

    void Abrir(ContextoApps& c) override {
        tela_ = Tela::Carregando;
        c.painel.MostrarStatus("Codex", PainelWatcher::Status::Carregando, "Consultando o Codex…");
        pedido_ = Pedido::Resumo;
    }

    void Girar(ContextoApps& c, int passo) override {
        if (tela_ != Tela::Carregando) {
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
            case Tela::Lista: {
                int ns = (int)sessoes_.size();
                if (i == 0) {
                    return false;  // Voltar
                }
                if (i == 1) {  // tarefa nova por voz
                    c.Perguntar("Quero começar uma tarefa nova no Codex do Mac. Pergunte em qual projeto e o que devo pedir; "
                                "depois confirme e use codex_nova_sessao.");
                    return true;
                }
                if (i == 2) {  // controle remoto
                    tela_ = Tela::Remoto;
                    c.painel.MostrarTexto("Controle remoto",
                                          std::string(remoto_ ? "Está ligado. " : "Está desligado. ") + remoto_detalhe_ +
                                              (remoto_ ? "\n\nDesligar o controle remoto do Codex?"
                                                       : "\n\nLigar o controle remoto do Codex?"),
                                          {remoto_ ? "Desligar" : "Ligar", "Voltar"});
                    return true;
                }
                if (i >= 3 && i < 3 + ns) {
                    atual_ = i - 3;
                    tela_ = Tela::Carregando;
                    c.painel.MostrarStatus(sessoes_[atual_].titulo, PainelWatcher::Status::Carregando, "Lendo as mensagens…");
                    pedido_ = Pedido::Sessao;
                    return true;
                }
                if (i >= 3 + ns && i < 3 + ns + (int)nuvem_.size()) {
                    atual_ = i - 3 - ns;
                    tela_ = Tela::Carregando;
                    c.painel.MostrarStatus(nuvem_[atual_].titulo, PainelWatcher::Status::Carregando, "Consultando a tarefa…");
                    pedido_ = Pedido::Nuvem;
                    return true;
                }
                return true;
            }
            case Tela::Sessao:
                if (i == 0) {
                    const auto& s = sessoes_[atual_];
                    c.Perguntar("Quero mandar uma mensagem para a sessão do Codex \"" + s.titulo + "\" (id " + s.id +
                                "). Pergunte o que devo enviar e, depois que eu responder, confirme e use codex_enviar nessa sessão.");
                } else {
                    MostrarLista(c);
                }
                return true;
            case Tela::Remoto:
                if (i == 0) {
                    tela_ = Tela::Carregando;
                    c.painel.MostrarStatus("Controle remoto", PainelWatcher::Status::Carregando,
                                           remoto_ ? "Desligando…" : "Ligando…");
                    pedido_ = Pedido::Remoto;
                } else {
                    MostrarLista(c);
                }
                return true;
            case Tela::Texto:
            case Tela::Resultado:
                pedido_ = Pedido::Resumo;  // volta à lista atualizada
                tela_ = Tela::Carregando;
                c.painel.MostrarStatus("Codex", PainelWatcher::Status::Carregando, "Atualizando…");
                return true;
        }
        return false;
    }

    bool Voltar(ContextoApps& c) override {
        if (tela_ == Tela::Sessao || tela_ == Tela::Remoto || tela_ == Tela::Texto || tela_ == Tela::Resultado) {
            MostrarLista(c);
            return true;
        }
        return false;
    }

    void Tique(ContextoApps& c) override {
        Pedido p = pedido_.exchange(Pedido::Nada);
        if (p == Pedido::Resumo) {
            BuscarResumo(c);
        } else if (p == Pedido::Sessao) {
            LerSessao(c);
        } else if (p == Pedido::Nuvem) {
            LerNuvem(c);
        } else if (p == Pedido::Remoto) {
            AlternarRemoto(c);
        }
    }

private:
    enum class Tela { Carregando, Erro, Lista, Sessao, Remoto, Texto, Resultado };
    enum class Pedido { Nada, Resumo, Sessao, Nuvem, Remoto };
    struct Item {
        std::string id, titulo, detalhe;
    };
    std::atomic<Tela> tela_{Tela::Carregando};
    std::atomic<Pedido> pedido_{Pedido::Nada};
    std::vector<Item> sessoes_, nuvem_;
    std::string erro_nuvem_;
    bool remoto_ = false;
    std::string remoto_detalhe_;
    int atual_ = 0;

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

    static void LerItens(cJSON* lista, std::vector<Item>& destino) {
        destino.clear();
        cJSON* item = nullptr;
        cJSON_ArrayForEach(item, lista) {
            destino.push_back({RedeWatcher::Campo(item, "id"), RedeWatcher::Campo(item, "titulo"),
                               RedeWatcher::Campo(item, "detalhe")});
        }
    }

    void BuscarResumo(ContextoApps& c) {
        std::string corpo;
        if (!RedeWatcher::Pedir("GET", "/watcher/codex", "", corpo)) {
            tela_ = Tela::Erro;
            c.painel.MostrarStatus("Codex", PainelWatcher::Status::Erro, "Não consegui falar com o Mac agora.", {"Voltar"});
            return;
        }
        cJSON* raiz = cJSON_Parse(corpo.c_str());
        cJSON* remoto = raiz ? cJSON_GetObjectItem(raiz, "remoto") : nullptr;
        remoto_ = cJSON_IsTrue(cJSON_GetObjectItem(remoto, "ligado"));
        remoto_detalhe_ = RedeWatcher::Campo(remoto, "detalhe");
        LerItens(raiz ? cJSON_GetObjectItem(raiz, "sessoes") : nullptr, sessoes_);
        LerItens(raiz ? cJSON_GetObjectItem(raiz, "nuvem") : nullptr, nuvem_);
        erro_nuvem_ = RedeWatcher::Campo(raiz, "erro_nuvem");
        cJSON_Delete(raiz);
        MostrarLista(c);
    }

    void MostrarLista(ContextoApps& c) {
        tela_ = Tela::Lista;
        std::vector<PainelWatcher::Item> itens = {
            {"Voltar", "", MATERIAL_SYMBOLS_ARROW_BACK},
            {"Nova tarefa", "Diga o que o Codex deve fazer", MATERIAL_SYMBOLS_MIC},
            {"Controle remoto", remoto_ ? "Ligado" : "Desligado", remoto_ ? MATERIAL_SYMBOLS_LOCK_OPEN : MATERIAL_SYMBOLS_LOCK}};
        for (const auto& s : sessoes_) {
            itens.push_back({s.titulo, s.detalhe, MATERIAL_SYMBOLS_ROBOT_2});
        }
        for (const auto& t : nuvem_) {
            itens.push_back({t.titulo, t.detalhe, MATERIAL_SYMBOLS_CLOUD_UPLOAD});
        }
        if (sessoes_.empty() && nuvem_.empty()) {
            itens.push_back({"Nenhuma sessão do Codex", erro_nuvem_, MATERIAL_SYMBOLS_INFO});
        }
        c.painel.MostrarLista("Codex", itens, 1);
    }

    void LerSessao(ContextoApps& c) {
        const auto& s = sessoes_[atual_];
        std::string corpo, texto;
        if (RedeWatcher::Pedir("GET", "/watcher/codex/sessoes/" + Codificar(s.id), "", corpo)) {
            cJSON* raiz = cJSON_Parse(corpo.c_str());
            cJSON* lista = raiz ? cJSON_GetObjectItem(raiz, "mensagens") : nullptr;  // fora do macro (sem ?: dentro)
            cJSON* m = nullptr;
            cJSON_ArrayForEach(m, lista) {
                std::string hora = RedeWatcher::Campo(m, "hora");
                texto += (texto.empty() ? "" : "\n\n") + RedeWatcher::Campo(m, "quem") + (hora.empty() ? "" : " · " + hora) +
                         "\n" + RedeWatcher::Campo(m, "texto");
            }
            cJSON_Delete(raiz);
        }
        tela_ = Tela::Sessao;
        c.painel.MostrarTexto(s.titulo, (s.detalhe.empty() ? "" : s.detalhe + "\n\n") +
                                            (texto.empty() ? "Sem mensagens registradas." : texto),
                              {"Enviar mensagem", "Voltar"});
        c.painel.RolarTextoParaFim();
    }

    void LerNuvem(ContextoApps& c) {
        const auto& t = nuvem_[atual_];
        std::string corpo, texto;
        if (RedeWatcher::Pedir("GET", "/watcher/codex/nuvem/" + Codificar(t.id), "", corpo)) {
            cJSON* raiz = cJSON_Parse(corpo.c_str());
            texto = RedeWatcher::Campo(raiz, "texto");
            cJSON_Delete(raiz);
        }
        tela_ = Tela::Texto;
        c.painel.MostrarTexto(t.titulo, texto.empty() ? "Não consegui ler a tarefa agora." : texto, {"Voltar"});
    }

    void AlternarRemoto(ContextoApps& c) {
        std::string corpo;
        bool ok = RedeWatcher::Pedir("POST", "/watcher/codex/remoto", remoto_ ? "{\"ligar\":false}" : "{\"ligar\":true,\"confirmado\":true}", corpo);
        cJSON* raiz = ok ? cJSON_Parse(corpo.c_str()) : nullptr;
        ok = ok && cJSON_IsTrue(cJSON_GetObjectItem(raiz, "ok"));
        std::string mensagem = RedeWatcher::Campo(raiz, "mensagem");
        cJSON_Delete(raiz);
        if (ok) {
            remoto_ = !remoto_;
        }
        tela_ = Tela::Resultado;
        c.painel.MostrarStatus("Controle remoto", ok ? PainelWatcher::Status::Sucesso : PainelWatcher::Status::Erro,
                               mensagem.empty() ? (ok ? "Pronto." : "Não consegui mudar agora.") : mensagem, {"Voltar"});
    }
};

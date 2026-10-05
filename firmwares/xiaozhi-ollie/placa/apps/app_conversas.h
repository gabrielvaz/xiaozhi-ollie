// App "Conversas": todas as conversas com o agente, guardadas no Mac (iCloud Drive/Watcher/Conversas).
// Lista com título e quando; abrir mostra o resumo e as falas (rolável) e "Ouvir" narra a conversa.
// Rede e áudio rodam no Tique (fora da trava da gaveta).
#pragma once

#include <atomic>
#include <vector>

#include "../nucleo_apps.h"

class AppConversas : public AppWatcher {
public:
    const char* Nome() const override { return "Conversas"; }
    const char* Icone() const override { return MATERIAL_SYMBOLS_CHAT_BUBBLE; }
    std::string Detalhe() const override { return "Ler ou ouvir de novo"; }

    void Abrir(ContextoApps& c) override {
        tela_ = Tela::Lista;
        c.painel.MostrarStatus("Conversas", PainelWatcher::Status::Carregando, "Buscando conversas…");
        pedido_ = Pedido::Lista;
    }

    void Girar(ContextoApps& c, int passo) override { c.painel.Mover(passo); }

    bool Clicar(ContextoApps& c) override {
        if (pedido_ != Pedido::Nada) {
            return true;  // ainda carregando
        }
        int i = c.painel.Selecionado();
        if (tela_ == Tela::Lista) {
            if (i <= 0 || i > (int)itens_.size()) {  // item 0 = Voltar
                return false;
            }
            atual_ = i - 1;
            c.painel.MostrarStatus(itens_[atual_].titulo, PainelWatcher::Status::Carregando, "Abrindo…");
            pedido_ = Pedido::Conversa;
            return true;
        }
        if (tela_ == Tela::Conversa && i == 0) {
            c.painel.MostrarStatus(titulo_, PainelWatcher::Status::Carregando, "Preparando o áudio…");
            pedido_ = Pedido::Audio;
            return true;
        }
        MostrarLista(c);
        return true;
    }

    bool Voltar(ContextoApps& c) override {
        if (tela_ == Tela::Conversa) {
            MostrarLista(c);
            return true;
        }
        return false;
    }

    void Tique(ContextoApps& c) override {
        Pedido p = pedido_;
        if (p == Pedido::Nada) {
            return;
        }
        std::string corpo;
        if (p == Pedido::Lista) {
            if (!RedeWatcher::Pedir("GET", "/watcher/conversas", "", corpo)) {
                pedido_ = Pedido::Nada;
                tela_ = Tela::Erro;
                c.painel.MostrarStatus("Conversas", PainelWatcher::Status::Erro,
                                       "Sem conexão com o Mac. Sem internet, o registro fica no microSD.", {"Voltar"});
                return;
            }
            itens_.clear();
            cJSON* raiz = cJSON_Parse(corpo.c_str());
            cJSON* lista = raiz ? cJSON_GetObjectItem(raiz, "conversas") : nullptr;
            cJSON* item = nullptr;
            cJSON_ArrayForEach(item, lista) {
                itens_.push_back({RedeWatcher::Campo(item, "id"), RedeWatcher::Campo(item, "titulo"),
                                  RedeWatcher::Campo(item, "detalhe")});
            }
            cJSON_Delete(raiz);
            atual_ = 0;
            pedido_ = Pedido::Nada;
            MostrarLista(c);
        } else if (p == Pedido::Conversa) {
            const auto& it = itens_[atual_];
            bool ok = RedeWatcher::Pedir("GET", "/watcher/conversas/" + it.id, "", corpo);
            cJSON* raiz = ok ? cJSON_Parse(corpo.c_str()) : nullptr;
            titulo_ = it.titulo;
            texto_ = raiz ? RedeWatcher::Campo(raiz, "texto") : "";
            cJSON_Delete(raiz);
            if (texto_.size() > 1500) {
                texto_ = texto_.substr(0, 1500) + "…";
            }
            pedido_ = Pedido::Nada;
            MostrarConversa(c, texto_.empty() ? "Não consegui abrir esta conversa." : texto_);
        } else if (p == Pedido::Audio) {
            som_.clear();
            bool ok = RedeWatcher::Pedir("GET", "/watcher/conversas/" + itens_[atual_].id + "/audio", "", som_) &&
                      som_.rfind("OggS", 0) == 0;
            pedido_ = Pedido::Nada;
            MostrarConversa(c, ok ? texto_ : "Não consegui gerar o áudio agora.\n\n" + texto_);
            if (ok) {
                ContextoApps::App().PlaySound(som_);
            }
        }
    }

private:
    enum class Tela { Lista, Conversa, Erro };
    enum class Pedido { Nada, Lista, Conversa, Audio };
    struct Item {
        std::string id, titulo, detalhe;
    };
    std::atomic<Tela> tela_{Tela::Lista};
    std::atomic<Pedido> pedido_{Pedido::Nada};
    std::vector<Item> itens_;
    int atual_ = 0;
    std::string titulo_, texto_;
    std::string som_;  // mantido vivo durante a reprodução

    void MostrarLista(ContextoApps& c) {
        tela_ = Tela::Lista;
        std::vector<PainelWatcher::Item> lista = {{"Voltar", "", MATERIAL_SYMBOLS_ARROW_BACK}};
        for (const auto& it : itens_) {
            lista.push_back({it.titulo, it.detalhe, MATERIAL_SYMBOLS_CHAT_BUBBLE});
        }
        if (itens_.empty()) {
            lista.push_back({"Nenhuma conversa ainda", "", MATERIAL_SYMBOLS_CHAT_BUBBLE});
        }
        c.painel.MostrarLista("Conversas", lista, itens_.empty() ? 0 : atual_ + 1);
    }

    void MostrarConversa(ContextoApps& c, const std::string& texto) {
        tela_ = Tela::Conversa;
        c.painel.MostrarTexto(titulo_, texto, {"Ouvir", "Voltar"});
    }
};

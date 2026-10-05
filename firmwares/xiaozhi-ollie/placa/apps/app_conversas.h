// App "Conversas": todas as conversas com o agente, guardadas no Mac (iCloud Drive/Watcher/Conversas).
// Lista com título e quando; abrir mostra o resumo e as falas (rolável) e "Ouvir" narra a conversa.
// Rede e áudio rodam no Tique (fora da trava da gaveta).
#pragma once

#include <atomic>
#include <vector>

#include "../idioma_watcher.h"
#include "../nucleo_apps.h"

class AppConversas : public AppWatcher {
public:
    const char* Nome() const override { return TR("Conversas", "Chats", "对话", "Conversaciones"); }
    const char* Icone() const override { return MATERIAL_SYMBOLS_CHAT_BUBBLE; }
    std::string Detalhe() const override { return TR("Ler ou ouvir de novo", "Read or listen again", "重新阅读或收听", "Leer o escuchar de nuevo"); }

    void Abrir(ContextoApps& c) override {
        tela_ = Tela::Lista;
        c.painel.MostrarStatus(TR("Conversas", "Chats", "对话", "Conversaciones"), PainelWatcher::Status::Carregando, TR("Buscando conversas…", "Loading chats…", "正在加载对话…", "Buscando conversaciones…"));
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
            c.painel.MostrarStatus(itens_[atual_].titulo, PainelWatcher::Status::Carregando, TR("Abrindo…", "Opening…", "正在打开…", "Abriendo…"));
            pedido_ = Pedido::Conversa;
            return true;
        }
        if (tela_ == Tela::Conversa && i == 0) {
            c.painel.MostrarStatus(titulo_, PainelWatcher::Status::Carregando, TR("Preparando o áudio…", "Preparing audio…", "正在准备音频…", "Preparando el audio…"));
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
                c.painel.MostrarStatus(TR("Conversas", "Chats", "对话", "Conversaciones"), PainelWatcher::Status::Erro,
                                       TR("Sem conexão com o Mac. Sem internet, o registro fica no microSD.", "Can't reach the Mac. Offline, the log stays on the microSD.", "无法连接 Mac。没有网络时，记录会保存在 microSD 卡上。", "Sin conexión con el Mac. Sin internet, el registro queda en la microSD."), {TR("Voltar", "Back", "返回", "Volver")});
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
            MostrarConversa(c, texto_.empty() ? TR("Não consegui abrir esta conversa.", "Couldn't open this chat.", "无法打开这段对话。", "No pude abrir esta conversación.") : texto_);
        } else if (p == Pedido::Audio) {
            som_.clear();
            bool ok = RedeWatcher::Pedir("GET", "/watcher/conversas/" + itens_[atual_].id + "/audio", "", som_) &&
                      som_.rfind("OggS", 0) == 0;
            pedido_ = Pedido::Nada;
            MostrarConversa(c, ok ? texto_ : TR("Não consegui gerar o áudio agora.\n\n", "Couldn't create the audio right now.\n\n", "暂时无法生成音频。\n\n", "No pude generar el audio ahora.\n\n") + texto_);
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
        std::vector<PainelWatcher::Item> lista = {{TR("Voltar", "Back", "返回", "Volver"), "", MATERIAL_SYMBOLS_ARROW_BACK}};
        for (const auto& it : itens_) {
            lista.push_back({it.titulo, it.detalhe, MATERIAL_SYMBOLS_CHAT_BUBBLE});
        }
        if (itens_.empty()) {
            lista.push_back({TR("Nenhuma conversa ainda", "No chats yet", "还没有对话", "Sin conversaciones"), "", MATERIAL_SYMBOLS_CHAT_BUBBLE});
        }
        c.painel.MostrarLista(TR("Conversas", "Chats", "对话", "Conversaciones"), lista, itens_.empty() ? 0 : atual_ + 1);
    }

    void MostrarConversa(ContextoApps& c, const std::string& texto) {
        tela_ = Tela::Conversa;
        c.painel.MostrarTexto(titulo_, texto, {TR("Ouvir", "Listen", "收听", "Escuchar"), TR("Voltar", "Back", "返回", "Volver")});
    }
};

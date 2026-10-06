// App "Avisos": histórico das notificações do Watcher (sessão esperando você, tarefa concluída,
// reunião pronta), guardado no Mac. Abrir um aviso mostra o texto e, se for de uma sessão, "Ir para a sessão".
#pragma once

#include <atomic>
#include <vector>

#include "../idioma_watcher.h"
#include "../nucleo_apps.h"

class AppAvisos : public AppWatcher {
public:
    const char* Nome() const override { return TR("Avisos", "Notices", "通知", "Avisos"); }
    const char* Id() const override { return "avisos"; }
    const char* Icone() const override { return MATERIAL_SYMBOLS_NOTIFICATIONS; }
    std::string Detalhe() const override {
        return TR("Histórico de notificações", "Notification history", "通知记录", "Historial de avisos");
    }

    void Abrir(ContextoApps& c) override {
        tela_ = Tela::Carregando;
        c.painel.MostrarStatus(TR("Avisos", "Notices", "通知", "Avisos"), PainelWatcher::Status::Carregando,
                             TR("Carregando avisos…", "Loading notices…", "正在加载通知…", "Cargando avisos…"));
        buscar_ = true;
    }

    void Girar(ContextoApps& c, int passo) override {
        if (tela_ != Tela::Carregando) {
            c.painel.Mover(passo);
        }
    }

    bool Clicar(ContextoApps& c) override {
        int i = c.painel.Selecionado();
        if (tela_ == Tela::Carregando) {
            return true;
        }
        if (tela_ == Tela::Lista) {
            if (i <= 0 || i > (int)avisos_.size()) {
                return false;  // Voltar
            }
            atual_ = i - 1;
            const auto& a = avisos_[atual_];
            tela_ = Tela::Item;
            c.painel.MostrarTexto(a.Nome(), a.Linha() + "\n\n" + a.texto,
                                  !a.TemSessao() ? std::vector<std::string>{TR("Voltar", "Back", "返回", "Volver")}
                                                 : std::vector<std::string>{TR("Continuar", "Continue", "继续", "Continuar"),
                                                                            TR("Voltar", "Back", "返回", "Volver")});
            return true;
        }
        // Continuar: abre a sessão no app Claude Code (mensagens, enviar pedido, responder pergunta)
        if (tela_ == Tela::Item && i == 0 && avisos_[atual_].TemSessao() && c.abrir_app) {
            const auto& a = avisos_[atual_];
            c.abrir_app("sessoes", "ir\n" + a.sessao + "\n" + a.Nome());
            return true;
        }
        MostrarLista(c);
        return true;
    }

    bool Voltar(ContextoApps& c) override {
        if (tela_ == Tela::Item) {
            MostrarLista(c);
            return true;
        }
        return false;
    }

    void Tique(ContextoApps& c) override {
        if (!buscar_.exchange(false)) {
            return;
        }
        std::string corpo;
        avisos_.clear();
        if (RedeWatcher::Pedir("GET", "/watcher/avisos/historico", "", corpo)) {
            cJSON* raiz = cJSON_Parse(corpo.c_str());
            cJSON* lista = raiz ? cJSON_GetObjectItem(raiz, "avisos") : nullptr;
            cJSON* a = nullptr;
            cJSON_ArrayForEach(a, lista) {
                avisos_.push_back({RedeWatcher::Campo(a, "titulo"), RedeWatcher::Campo(a, "texto"),
                                   RedeWatcher::Campo(a, "detalhe"), RedeWatcher::Campo(a, "tipo"),
                                   RedeWatcher::Campo(a, "sessao"), RedeWatcher::Campo(a, "nome_sessao")});
            }
            cJSON_Delete(raiz);
        }
        MostrarLista(c);
    }

private:
    enum class Tela { Carregando, Lista, Item };
    struct Aviso {
        std::string titulo, texto, detalhe, tipo, sessao, nome_sessao;

        // Título da lista: o nome da sessão (vários "Tarefa concluída" ficavam iguais). Avisos antigos sem o
        // campo usam o nome que vem antes de ":" no texto.
        std::string Nome() const {
            if (!nome_sessao.empty()) {
                return nome_sessao;
            }
            auto pos = texto.find(": ");
            return (tipo == "concluiu" && pos != std::string::npos && pos < 40) ? texto.substr(0, pos) : titulo;
        }
        bool TemSessao() const { return !sessao.empty() || Nome() != titulo; }
        // Linha de baixo: quando e o tipo do aviso ("Hoje 19:30 · Tarefa concluída")
        std::string Linha() const { return Nome() == titulo ? detalhe : detalhe + " · " + titulo; }
    };
    std::atomic<Tela> tela_{Tela::Carregando};
    std::atomic<bool> buscar_{false};
    std::vector<Aviso> avisos_;
    int atual_ = 0;

    static const char* IconeTipo(const std::string& tipo) {
        if (tipo == "esperando") return MATERIAL_SYMBOLS_WARNING;
        if (tipo == "reuniao") return MATERIAL_SYMBOLS_MIC;
        return MATERIAL_SYMBOLS_CHECK_CIRCLE;  // tarefa concluída e outros
    }

    void MostrarLista(ContextoApps& c) {
        tela_ = Tela::Lista;
        std::vector<PainelWatcher::Item> itens = {{TR("Voltar", "Back", "返回", "Volver"), "", MATERIAL_SYMBOLS_ARROW_BACK}};
        for (const auto& a : avisos_) {
            itens.push_back({a.Nome(), a.Linha(), IconeTipo(a.tipo)});
        }
        if (avisos_.empty()) {
            itens.push_back({TR("Nenhum aviso ainda", "No notices yet", "暂无通知", "Aún no hay avisos"), "", MATERIAL_SYMBOLS_NOTIFICATIONS});
        }
        c.painel.MostrarLista(TR("Avisos", "Notices", "通知", "Avisos"), itens, avisos_.empty() ? 0 : 1);
    }
};

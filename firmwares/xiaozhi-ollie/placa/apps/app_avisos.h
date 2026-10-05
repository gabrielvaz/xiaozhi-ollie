// App "Avisos": histórico das notificações do Watcher (sessão esperando você, tarefa concluída,
// reunião pronta), guardado no Mac. Abrir um aviso mostra o texto e, se for de uma sessão, "Ir para a sessão".
#pragma once

#include <atomic>
#include <vector>

#include "../nucleo_apps.h"

class AppAvisos : public AppWatcher {
public:
    const char* Nome() const override { return "Avisos"; }
    const char* Id() const override { return "avisos"; }
    const char* Icone() const override { return MATERIAL_SYMBOLS_NOTIFICATIONS; }
    std::string Detalhe() const override { return "Histórico de notificações"; }

    void Abrir(ContextoApps& c) override {
        tela_ = Tela::Carregando;
        c.painel.MostrarStatus("Avisos", PainelWatcher::Status::Carregando, "Carregando avisos…");
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
            c.painel.MostrarTexto(a.titulo, a.detalhe + "\n\n" + a.texto,
                                  a.sessao.empty() ? std::vector<std::string>{"Voltar"}
                                                   : std::vector<std::string>{"Ir para a sessão", "Voltar"});
            return true;
        }
        if (tela_ == Tela::Item && i == 0 && !avisos_[atual_].sessao.empty() && c.abrir_app) {
            c.abrir_app("sessoes", "ir\n" + avisos_[atual_].sessao);
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
                                   RedeWatcher::Campo(a, "sessao")});
            }
            cJSON_Delete(raiz);
        }
        MostrarLista(c);
    }

private:
    enum class Tela { Carregando, Lista, Item };
    struct Aviso {
        std::string titulo, texto, detalhe, tipo, sessao;
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
        std::vector<PainelWatcher::Item> itens = {{"Voltar", "", MATERIAL_SYMBOLS_ARROW_BACK}};
        for (const auto& a : avisos_) {
            itens.push_back({a.titulo, a.detalhe + " · " + a.texto, IconeTipo(a.tipo)});
        }
        if (avisos_.empty()) {
            itens.push_back({"Nenhum aviso ainda", "", MATERIAL_SYMBOLS_NOTIFICATIONS});
        }
        c.painel.MostrarLista("Avisos", itens, avisos_.empty() ? 0 : 1);
    }
};

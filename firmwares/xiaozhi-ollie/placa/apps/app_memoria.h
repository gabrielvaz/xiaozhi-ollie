// App "Memória offline": itens (sessões, últimas reuniões, lembretes) com texto e áudio,
// baixados do Mac para o microSD a cada 30 min. Funciona sem internet.
#pragma once

#include <algorithm>
#include <atomic>
#include <vector>

#include "../nucleo_apps.h"

class AppMemoria : public AppWatcher {
public:
    const char* Nome() const override { return "Memória"; }
    const char* Icone() const override { return MATERIAL_SYMBOLS_MEMORY; }
    std::string Detalhe() const override { return "Funciona sem internet"; }

    void Abrir(ContextoApps& c) override {
        Carregar();
        if (itens_.empty()) {
            tela_ = Tela::Vazia;
            c.painel.MostrarTexto("Memória offline",
                                  CartaoWatcher::Instancia().Montado()
                                      ? "Ainda vazia. Ela se enche sozinha enquanto houver internet."
                                      : "Sem microSD. Coloque um cartão para usar a memória offline.",
                                  {"Voltar"});
            return;
        }
        MostrarLista(c, 0);
    }

    void Girar(ContextoApps& c, int passo) override { c.painel.Mover(passo); }

    bool Clicar(ContextoApps& c) override {
        int i = c.painel.Selecionado();
        if (tela_ == Tela::Vazia) {
            return false;
        }
        if (tela_ == Tela::Lista) {
            if (i <= 0 || i > (int)itens_.size()) {  // item 0 = Voltar
                return false;
            }
            atual_ = i - 1;
            tela_ = Tela::Item;
            const auto& m = itens_[atual_];  // item 0 da lista é o Voltar
            c.painel.MostrarTexto(m.titulo, m.texto.size() > 600 ? m.texto.substr(0, 597) + "…" : m.texto,
                                  m.audio.empty() ? std::vector<std::string>{"Voltar"}
                                                  : std::vector<std::string>{"Ouvir", "Voltar"});
            return true;
        }
        if (i == 0 && !itens_[atual_].audio.empty()) {
            tocar_ = std::string(CartaoWatcher::kPasta) + "/memoria/" + itens_[atual_].audio;
        } else {
            MostrarLista(c, atual_);
        }
        return true;
    }

    bool Voltar(ContextoApps& c) override {
        if (tela_ == Tela::Item) {
            MostrarLista(c, atual_);
            return true;
        }
        return false;
    }

    void Tique(ContextoApps& c) override {
        if (tocar_.empty()) {
            return;
        }
        std::string caminho;
        caminho.swap(tocar_);
        if (CartaoWatcher::Ler(caminho, som_)) {
            ContextoApps::App().PlaySound(som_);
        }
    }

    // Sincroniza a cada 30 min quando o aparelho está parado e online
    void Fundo(ContextoApps& c) override {
        int agora = ContextoApps::Agora();
        if (agora - ultima_sincronia_ < kIntervaloS || !RedeWatcher::Online() ||
            ContextoApps::App().GetDeviceState() != kDeviceStateIdle) {
            return;
        }
        ultima_sincronia_ = agora;
        Sincronizar();
    }

private:
    enum class Tela { Vazia, Lista, Item };
    struct Item {
        std::string titulo, detalhe, texto, audio;
    };
    static constexpr int kIntervaloS = 30 * 60;
    std::atomic<Tela> tela_{Tela::Vazia};
    std::vector<Item> itens_;
    int atual_ = 0;
    int ultima_sincronia_ = -kIntervaloS + 90;  // primeira sincronia ~90 s após ligar
    std::string tocar_;
    std::string som_;  // mantido vivo durante a reprodução

    static std::string Pasta() { return std::string(CartaoWatcher::kPasta) + "/memoria/"; }

    void Carregar() {
        itens_.clear();
        std::string json;
        if (!CartaoWatcher::Instancia().Montado() || !CartaoWatcher::Ler(Pasta() + "memoria.json", json)) {
            return;
        }
        cJSON* raiz = cJSON_Parse(json.c_str());
        cJSON* lista = raiz ? cJSON_GetObjectItem(raiz, "itens") : nullptr;
        cJSON* item = nullptr;
        cJSON_ArrayForEach(item, lista) {
            itens_.push_back({RedeWatcher::Campo(item, "titulo"), RedeWatcher::Campo(item, "detalhe"),
                              RedeWatcher::Campo(item, "texto"), RedeWatcher::Campo(item, "audio")});
        }
        cJSON_Delete(raiz);
    }

    void MostrarLista(ContextoApps& c, int selecionar) {
        tela_ = Tela::Lista;
        std::vector<PainelWatcher::Item> lista = {{"Voltar", "", MATERIAL_SYMBOLS_ARROW_BACK}};
        for (const auto& m : itens_) {
            lista.push_back({m.titulo, m.detalhe, MATERIAL_SYMBOLS_CHAT_BUBBLE});
        }
        c.painel.MostrarLista("Memória offline", lista, selecionar + 1);
    }

    void Sincronizar() {
        std::string corpo;
        if (!CartaoWatcher::Instancia().Montado() || !RedeWatcher::Pedir("GET", "/watcher/memoria", "", corpo)) {
            return;
        }
        cJSON* raiz = cJSON_Parse(corpo.c_str());
        if (raiz == nullptr) {
            return;
        }
        std::vector<std::string> usados = {"memoria.json"};
        cJSON* lista = cJSON_GetObjectItem(raiz, "itens");
        cJSON* item = nullptr;
        cJSON_ArrayForEach(item, lista) {
            std::string audio = RedeWatcher::Campo(item, "audio");
            if (audio.empty()) {
                continue;
            }
            usados.push_back(audio);
            if (CartaoWatcher::Tamanho(Pasta() + audio) > 0) {
                continue;
            }
            std::string dados;
            if (RedeWatcher::Pedir("GET", "/watcher/memoria/audio/" + audio, "", dados) && dados.rfind("OggS", 0) == 0) {
                CartaoWatcher::Escrever(Pasta() + audio, dados);
            }
        }
        cJSON_Delete(raiz);
        CartaoWatcher::Escrever(Pasta() + "memoria.json", corpo);
        for (const auto& nome : CartaoWatcher::Listar(Pasta())) {  // apaga áudios antigos
            if (std::find(usados.begin(), usados.end(), nome) == usados.end()) {
                remove((Pasta() + nome).c_str());
            }
        }
    }
};

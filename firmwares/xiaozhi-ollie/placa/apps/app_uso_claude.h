// App "Uso do Claude": limites de 5 h e da semana (lidos no Mac; o token não vem ao aparelho).
#pragma once

#include <atomic>

#include "../nucleo_apps.h"

class AppUsoClaude : public AppWatcher {
public:
    const char* Nome() const override { return "Uso Claude"; }
    const char* Icone() const override { return MATERIAL_SYMBOLS_PROGRESS_ACTIVITY; }
    std::string Detalhe() const override { return "Limites de 5 h e da semana"; }

    void Abrir(ContextoApps& c) override {
        c.painel.MostrarStatus("Uso do Claude", PainelWatcher::Status::Carregando, "Consultando o uso…");
        buscar_ = true;
    }

    bool Clicar(ContextoApps& c) override { return false; }  // qualquer clique volta

    void Tique(ContextoApps& c) override {
        if (!buscar_.exchange(false)) {
            return;
        }
        std::string corpo;
        if (!RedeWatcher::Pedir("GET", "/watcher/uso", "", corpo)) {
            c.painel.MostrarTexto("Uso do Claude", "Não consegui falar com o Mac agora.", {"Voltar"});
            return;
        }
        cJSON* raiz = cJSON_Parse(corpo.c_str());
        if (raiz && cJSON_IsTrue(cJSON_GetObjectItem(raiz, "ok"))) {
            c.painel.MostrarUso(RedeWatcher::Numero(raiz, "pct_5h"), RedeWatcher::Numero(raiz, "reinicio_5h_s"),
                                RedeWatcher::Numero(raiz, "pct_7d"), RedeWatcher::Numero(raiz, "reinicio_7d_s"));
        } else {
            std::string erro = RedeWatcher::Campo(raiz, "erro");
            c.painel.MostrarTexto("Uso do Claude", erro.empty() ? "Não consegui ler o uso agora." : erro, {"Voltar"});
        }
        cJSON_Delete(raiz);
    }

private:
    std::atomic<bool> buscar_{false};
};

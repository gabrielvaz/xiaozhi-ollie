// App "Previsão do tempo": tela determinística (sem IA) com ícones — agora, hoje e amanhã —
// a partir de GET /watcher/tempo (local pelo IP da conexão). Botão "Ollie, fala" lê a previsão em voz.
#pragma once

#include <atomic>
#include <vector>

#include "../nucleo_apps.h"

class AppTempo : public AppWatcher {
public:
    const char* Nome() const override { return "Tempo"; }
    const char* Icone() const override { return MATERIAL_SYMBOLS_DEVICE_THERMOSTAT; }
    std::string Detalhe() const override { return "Agora, hoje e amanhã"; }

    void Abrir(ContextoApps& c) override {
        c.painel.MostrarStatus("Previsão do tempo", PainelWatcher::Status::Carregando, "Consultando…");
        buscar_ = true;
    }

    void Girar(ContextoApps& c, int passo) override { c.painel.Mover(passo); }

    bool Clicar(ContextoApps& c) override {
        if (!pronto_) {
            return !erro_;  // erro: clique volta
        }
        if (c.painel.Selecionado() == 0) {
            c.Perguntar("Me fala a previsão do tempo de hoje e amanhã aqui onde estou.");
            return true;
        }
        return false;
    }

    void Tique(ContextoApps& c) override {
        if (!buscar_.exchange(false)) {
            return;
        }
        pronto_ = erro_ = false;
        std::string corpo;
        cJSON* raiz = RedeWatcher::Pedir("GET", "/watcher/tempo", "", corpo) ? cJSON_Parse(corpo.c_str()) : nullptr;
        if (raiz == nullptr || !cJSON_IsTrue(cJSON_GetObjectItem(raiz, "ok"))) {
            std::string erro = RedeWatcher::Campo(raiz, "erro");
            erro_ = true;
            c.painel.MostrarStatus("Previsão do tempo", PainelWatcher::Status::Erro,
                                   erro.empty() ? "Não consegui falar com o Mac agora." : erro, {"Voltar"});
            cJSON_Delete(raiz);
            return;
        }
        std::vector<PainelWatcher::DiaTempo> dias;
        cJSON* lista = cJSON_GetObjectItem(raiz, "dias");
        cJSON* d = nullptr;
        cJSON_ArrayForEach(d, lista) {
            dias.push_back({RedeWatcher::Campo(d, "nome"), RedeWatcher::Campo(d, "categoria"), RedeWatcher::Numero(d, "min"),
                            RedeWatcher::Numero(d, "max"), RedeWatcher::Numero(d, "chuva", 0)});
        }
        c.painel.MostrarTempo(RedeWatcher::Campo(raiz, "local"), RedeWatcher::Numero(raiz, "temp"),
                              RedeWatcher::Campo(raiz, "categoria"), cJSON_IsTrue(cJSON_GetObjectItem(raiz, "noite")),
                              RedeWatcher::Campo(raiz, "descricao"), RedeWatcher::Numero(raiz, "sensacao"),
                              RedeWatcher::Numero(raiz, "umidade"), dias, {"Ollie, fala", "Voltar"});
        pronto_ = true;
        cJSON_Delete(raiz);
    }

private:
    std::atomic<bool> buscar_{false};
    std::atomic<bool> pronto_{false};
    std::atomic<bool> erro_{false};
};

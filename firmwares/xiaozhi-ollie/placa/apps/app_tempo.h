// App "Previsão do tempo": tela determinística (sem IA) com ícones — agora, hoje e amanhã —
// a partir de GET /watcher/tempo (local pelo IP da conexão). Botão "Ollie, fala" lê a previsão em voz.
#pragma once

#include <atomic>
#include <vector>

#include "../agente_watcher.h"
#include "../idioma_watcher.h"
#include "../nucleo_apps.h"

class AppTempo : public AppWatcher {
public:
    const char* Nome() const override { return TR("Tempo", "Weather", "天气", "El tiempo"); }
    const char* Id() const override { return "tempo"; }
    const char* Icone() const override { return PainelWatcher::kIconeSol; }  // sol desenhado
    std::string Detalhe() const override { return TR("Agora, hoje e amanhã", "Now, today and tomorrow", "现在、今天和明天", "Ahora, hoy y mañana"); }

    void Abrir(ContextoApps& c) override {
        c.painel.MostrarStatus(TR("Previsão do tempo", "Forecast", "天气预报", "Previsión"), PainelWatcher::Status::Carregando,
                               TR("Consultando…", "Checking…", "正在查询…", "Consultando…"), {}, "sunny");
        buscar_ = true;
    }

    void Girar(ContextoApps& c, int passo) override { c.painel.Mover(passo); }

    bool Clicar(ContextoApps& c) override {
        if (!pronto_) {
            return !erro_;  // erro: clique volta
        }
        if (c.painel.Selecionado() == 0) {
            c.Perguntar(TR("Me fala a previsão do tempo de hoje e amanhã aqui onde estou.",
                           "Tell me the weather forecast for today and tomorrow where I am.",
                           "告诉我我所在地今天和明天的天气预报。",
                           "Dime la previsión del tiempo de hoy y mañana donde estoy."));
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
        cJSON* raiz = RedeWatcher::PedirCache("/watcher/tempo", 600, corpo) ? cJSON_Parse(corpo.c_str()) : nullptr;
        if (raiz == nullptr || !cJSON_IsTrue(cJSON_GetObjectItem(raiz, "ok"))) {
            std::string erro = RedeWatcher::Campo(raiz, "erro");
            erro_ = true;
            c.painel.MostrarStatus(TR("Previsão do tempo", "Forecast", "天气预报", "Previsión"), PainelWatcher::Status::Erro,
                                   erro.empty() ? TR("Não consegui falar com o Mac agora.", "Couldn't reach the Mac right now.",
                                                         "暂时连不上 Mac。", "No he podido conectar con el Mac.")
                                                    : erro,
                                   {TR("Voltar", "Back", "返回", "Volver")});
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
                              RedeWatcher::Numero(raiz, "umidade"), dias,
                              {AgenteWatcher::Nome() + TR(", fala", ", read it", "，读一下", ", léelo"), TR("Voltar", "Back", "返回", "Volver")});
        pronto_ = true;
        cJSON_Delete(raiz);
    }

private:
    std::atomic<bool> buscar_{false};
    std::atomic<bool> pronto_{false};
    std::atomic<bool> erro_{false};
};

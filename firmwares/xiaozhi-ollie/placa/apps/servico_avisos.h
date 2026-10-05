// Serviço (sem tela na gaveta): busca avisos no Mac a cada 20 s — sessão esperando você,
// tarefa concluída, reunião pronta — e mostra só na tela. Liga/desliga em Configurações.
#pragma once

#include "../nucleo_apps.h"

class ServicoAvisos : public AppWatcher {
public:
    const char* Nome() const override { return "Avisos"; }
    bool Visivel() const override { return false; }
    void Abrir(ContextoApps& c) override {}
    bool Clicar(ContextoApps& c) override { return false; }

    void Fundo(ContextoApps& c) override {
        int agora = ContextoApps::Agora();
        if (agora - ultima_ < 20 || (c.gaveta_aberta && c.gaveta_aberta()) ||
            ContextoApps::App().GetDeviceState() != kDeviceStateIdle || !RedeWatcher::Online()) {
            return;
        }
        ultima_ = agora;
        std::string corpo;
        if (!RedeWatcher::Pedir("GET", "/watcher/avisos?desde=" + std::to_string(ultimo_id_), "", corpo)) {
            return;
        }
        cJSON* raiz = cJSON_Parse(corpo.c_str());
        if (raiz == nullptr) {
            return;
        }
        cJSON* lista = cJSON_GetObjectItem(raiz, "avisos");
        int n = cJSON_GetArraySize(lista);
        if (n > 0 && ConfigWatcher::Int("avisos", 1)) {
            cJSON* a = cJSON_GetArrayItem(lista, n - 1);  // o mais recente
            std::string titulo = RedeWatcher::Campo(a, "titulo"), texto = RedeWatcher::Campo(a, "texto");
            CartaoWatcher::Instancia().RegistrarConversa("aviso", titulo + ": " + texto);
            ContextoApps::Avisar(titulo, texto, RedeWatcher::Campo(a, "emocao"));  // só tela, sem som
        }
        ultimo_id_ = RedeWatcher::Numero(raiz, "ultimo", ultimo_id_);
        cJSON_Delete(raiz);
    }

private:
    int ultima_ = 0;
    int ultimo_id_ = -1;  // -1 = primeira consulta só sincroniza
};

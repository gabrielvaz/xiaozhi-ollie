// Serviço (sem tela na gaveta): busca avisos no Mac a cada 20 s — sessão esperando você,
// tarefa concluída, reunião pronta — e mostra na tela com um som curto (histórico no app Avisos). Sessão esperando você ganha o botão
// "Ir para a sessão" (abre o app Sessões já nela). Liga/desliga em Configurações.
#pragma once

#include "assets/lang_config.h"

#include "../idioma_watcher.h"
#include "../nucleo_apps.h"

class ServicoAvisos : public AppWatcher {
public:
    const char* Nome() const override { return TR("Avisos", "Notices", "通知", "Avisos"); }
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
            ContextoApps::Avisar(titulo, texto, RedeWatcher::Campo(a, "emocao"), Lang::Sounds::OGG_POPUP);  // "plim" curto
            std::string sessao = RedeWatcher::Campo(a, "sessao");
            // Todo aviso ganha um botão: de sessão (esperando ou concluída) -> "Continuar" leva à sessão;
            // reunião pronta -> abre o Gravador na lista
            std::string tipo = RedeWatcher::Campo(a, "tipo");
            std::string nome = RedeWatcher::Campo(a, "nome_sessao");
            if (!sessao.empty() && c.abrir_app) {
                c.abrir_app("sessoes", "aviso\n" + sessao + "\n" + (nome.empty() ? titulo : nome) + "\n" +
                                           (nome.empty() ? texto : titulo + "\n\n" + texto));
            } else if (tipo == "reuniao" && c.abrir_app) {
                c.abrir_app("gravador", "");
            }
        }
        ultimo_id_ = RedeWatcher::Numero(raiz, "ultimo", ultimo_id_);
        cJSON* sessoes = cJSON_GetObjectItem(raiz, "sessoes");
        if (cJSON_IsArray(sessoes)) {
            if (char* texto = cJSON_PrintUnformatted(sessoes)) {
                ContextoApps::sessoes_json = std::string("{\"sessoes\":") + texto + "}";
                ContextoApps::sessoes_quando = ContextoApps::Agora();
                cJSON_free(texto);
            }
        }
        cJSON* atividade = cJSON_GetObjectItem(raiz, "atividade");
        if (cJSON_IsObject(atividade)) {
            ContextoApps::atividade_n = RedeWatcher::Numero(atividade, "trabalhando", 0);
            cJSON* titulos = cJSON_GetObjectItem(atividade, "titulos");
            cJSON* primeiro = cJSON_IsArray(titulos) ? cJSON_GetArrayItem(titulos, 0) : nullptr;
            ContextoApps::atividade_titulo = cJSON_IsString(primeiro) ? primeiro->valuestring : "";
            // Há quantos segundos a 1ª sessão roda (o servidor lê o pedido no histórico do Claude Code)
            cJSON* segundos = cJSON_GetObjectItem(atividade, "segundos");
            cJSON* s0 = cJSON_IsArray(segundos) ? cJSON_GetArrayItem(segundos, 0) : nullptr;
            ContextoApps::atividade_inicio =
                cJSON_IsNumber(s0) && s0->valueint >= 0 ? ContextoApps::Agora() - s0->valueint : -1;
        }
        cJSON_Delete(raiz);
    }

private:
    int ultima_ = 0;
    int ultimo_id_ = -1;  // -1 = primeira consulta só sincroniza
};

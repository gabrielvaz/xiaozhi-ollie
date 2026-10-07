// Serviço (sem tela na gaveta): busca avisos no Mac a cada 20 s, sessão esperando você,
// tarefa concluída, reunião pronta, e mostra na tela com um som curto (histórico no app Avisos).
// Só "esperando você" interrompe (abre o app Sessões já na pergunta); o resto aparece na espera e sai
// sozinho. Vários avisos juntos viram uma lista. Liga/desliga em Configurações.
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
        // Tela acesa: a cada 20 s (avisos e sessões trabalhando em dia); apagada: a cada 5 min (bateria).
        // Economia de energia: 1 min com a tela acesa e 10 min apagada
        bool economia = ConfigWatcher::Int("economia", 0);
        int intervalo = ContextoApps::tela_apagada ? (economia ? 600 : 300) : (economia ? 60 : 20);
        if (agora - ultima_ < intervalo || (c.gaveta_aberta && c.gaveta_aberta()) ||
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
            // Todos os avisos do lote vão para o registro do cartão; a tela mostra um só ou a lista
            cJSON* urgente = nullptr;  // o mais recente com "esperando você"
            std::string linhas;
            for (int k = 0; k < n; k++) {
                cJSON* a = cJSON_GetArrayItem(lista, k);
                CartaoWatcher::Instancia().RegistrarConversa(
                    "aviso", RedeWatcher::Campo(a, "titulo") + ": " + RedeWatcher::Campo(a, "texto"));
                if (RedeWatcher::Campo(a, "tipo") == "esperando" && !RedeWatcher::Campo(a, "sessao").empty()) {
                    urgente = a;
                }
                if (n > 1) {
                    std::string linha = RedeWatcher::Campo(a, "titulo") + ": " + RedeWatcher::Campo(a, "texto");
                    if (linha.size() > 80) {
                        size_t corte = 79;
                        while (corte > 0 && (static_cast<unsigned char>(linha[corte]) & 0xC0) == 0x80) {
                            corte--;  // não corta um caractere UTF-8 ao meio
                        }
                        linha = linha.substr(0, corte) + "…";
                    }
                    linhas += (linhas.empty() ? "" : "\n") + linha;
                }
            }
            cJSON* a = urgente != nullptr ? urgente : cJSON_GetArrayItem(lista, n - 1);
            std::string titulo = RedeWatcher::Campo(a, "titulo"), texto = RedeWatcher::Campo(a, "texto");
            std::string tipo = RedeWatcher::Campo(a, "tipo");
            // Só "esperando você" segura a tela por 10 min; o resto sai em 2 (a espera volta por cima)
            int prende_s = tipo == "esperando" ? 600 : 120;
            if (n > 1) {
                char cabecalho[48];
                snprintf(cabecalho, sizeof(cabecalho), TR("%d avisos", "%d notices", "%d 条通知", "%d avisos"), n);
                ContextoApps::Avisar(cabecalho, linhas, RedeWatcher::Campo(a, "emocao"), Lang::Sounds::OGG_POPUP,
                                     prende_s);
            } else {
                ContextoApps::Avisar(titulo, texto, RedeWatcher::Campo(a, "emocao"), Lang::Sounds::OGG_POPUP, prende_s);
            }
            // Só uma sessão esperando resposta interrompe (abre o app Sessões já na pergunta); tarefa
            // concluída e reunião pronta ficam no aviso e no histórico, sem roubar a tela
            if (urgente != nullptr && c.abrir_app) {
                std::string sessao = RedeWatcher::Campo(urgente, "sessao");
                std::string nome = RedeWatcher::Campo(urgente, "nome_sessao");
                titulo = RedeWatcher::Campo(urgente, "titulo");
                texto = RedeWatcher::Campo(urgente, "texto");
                c.abrir_app("sessoes", "aviso\n" + sessao + "\n" + (nome.empty() ? titulo : nome) + "\n" +
                                           (nome.empty() ? texto : titulo + "\n\n" + texto));
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
            // Distintivo laranja na tela de espera: quantas sessões esperam uma resposta sua
            int esperando = 0;
            cJSON* s = nullptr;
            cJSON_ArrayForEach(s, sessoes) {
                if (RedeWatcher::Campo(s, "situacao") == "Esperando você") {
                    esperando++;
                }
            }
            if (esperando != ContextoApps::esperando_n.exchange(esperando)) {
                ContextoApps::App().Schedule(
                    [esperando]() { Board::GetInstance().GetDisplay()->MostrarPendencias(esperando); });
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

// Registro dos apps da gaveta, na ordem em que aparecem.
// Para criar um app: copie apps/app_tempo.h (o mais simples) ou apps/app_cronometro.h,
// implemente a classe e acrescente uma linha abaixo. Veja placa/README.md.
#pragma once

#include "apps/app_avisos.h"
#include "apps/app_camera.h"
#include "apps/app_cartao.h"
#include "apps/app_codex.h"
#include "apps/app_configuracoes.h"
#include "apps/app_contagem.h"
#include "apps/app_conversas.h"
#include "apps/app_cronometro.h"
#include "apps/app_memoria.h"
#include "apps/app_multica.h"
#include "apps/app_qrcode.h"
#include "apps/app_relogio_mundial.h"
#include "apps/app_reuniao.h"
#include "apps/app_sessoes.h"
#include "apps/app_tempo.h"
#include "apps/app_uso_claude.h"
#include "apps/servico_avisos.h"
#include "apps/servico_carregador.h"
#include "apps/servico_diagnostico.h"
#include "apps/servico_saudacao.h"
#include "apps/servico_silencio.h"
#include "gaveta_watcher.h"
#include "mcp_server.h"

inline void RegistrarApps(GavetaWatcher& gaveta) {
    DiagnosticoWatcher::Iniciar();  // motivo do último reinício e rastro anterior
    DiagnosticoWatcher::IniciarVigia();  // reinicia sozinho se a tela ou os apps travarem
    // O Ollie pode abrir um app da gaveta enquanto responde (ex.: pediu o tempo -> abre o app Tempo)
    GavetaWatcher* g = &gaveta;
    McpServer::GetInstance().AddTool(
        "self.app.open",
        "Open an app on the Watcher screen while you answer (the user sees it). Use it together with your spoken "
        "answer whenever the request matches an app. app must be one of: "
        "tempo (weather forecast), sessoes (Claude Code sessions: status, waiting for user, finished), "
        "uso_claude (Claude plan usage limits), codex (Codex sessions), multica (Multica issues/agents), "
        "gravador (voice recordings and notes; also to start recording), conversas (past chats with you), "
        "avisos (notifications history), cronometro (stopwatch), timer (countdown), relogios (world clock), "
        "camera (take a photo and see what the AI sees, photo gallery), qrcode (QR codes / Wi-Fi), memoria (offline memory), backup (copy microSD and chats to the Mac), "
        "ajustes (settings).",
        PropertyList({Property("app", kPropertyTypeString)}),
        [g](const PropertyList& propriedades) -> ReturnValue {
            std::string app = propriedades["app"].value<std::string>();
            g->AbrirApp(app, "");
            DiagnosticoWatcher::Marcar("ollie abre %s", app.c_str());
            return std::string("{\"ok\": true, \"app\": \"") + app + "\"}";
        });
    ContextoApps::App().DefinirVoz(ConfigWatcher::Int("voz", 1));  // Configurações > Respostas faladas
    // JSON das respostas do servidor (milhares de pedacinhos) na PSRAM: a memória interna fragmentava
    static cJSON_Hooks ganchos = {
        [](size_t n) -> void* {
            void* p = heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
            return p ? p : malloc(n);
        },
        [](void* p) { free(p); }};
    cJSON_InitHooks(&ganchos);
    gaveta.Registrar(std::make_unique<AppSessoes>());
    gaveta.Registrar(std::make_unique<AppCodex>());
    gaveta.Registrar(std::make_unique<AppMultica>());
    gaveta.Registrar(std::make_unique<AppAvisos>());
    gaveta.Registrar(std::make_unique<AppCamera>());
    gaveta.Registrar(std::make_unique<AppConversas>());
    gaveta.Registrar(std::make_unique<AppUsoClaude>());
    gaveta.Registrar(std::make_unique<AppTempo>());
    gaveta.Registrar(std::make_unique<AppReuniao>());
    gaveta.Registrar(std::make_unique<AppCronometro>());
    gaveta.Registrar(std::make_unique<AppContagem>());
    gaveta.Registrar(std::make_unique<AppRelogioMundial>());
    gaveta.Registrar(std::make_unique<AppQrCode>());
    gaveta.Registrar(std::make_unique<AppMemoria>());
    gaveta.Registrar(std::make_unique<AppCartao>());
    gaveta.Registrar(std::make_unique<AppConfiguracoes>());
    gaveta.Registrar(std::make_unique<ServicoAvisos>());  // sem tela: avisos do Mac
    gaveta.Registrar(std::make_unique<ServicoSaudacao>());  // sem tela: saudação na espera
    gaveta.Registrar(std::make_unique<ServicoSilencio>());  // sem tela: para de ouvir se ninguém falar
    gaveta.Registrar(std::make_unique<ServicoCarregador>());  // sem tela: choque ao conectar o cabo
    gaveta.Registrar(std::make_unique<ServicoDiagnostico>());  // sem tela: pulso e rastro de travamentos
}

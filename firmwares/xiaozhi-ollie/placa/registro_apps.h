// Registro dos apps da gaveta, na ordem em que aparecem.
// Para criar um app: copie apps/app_tempo.h (o mais simples) ou apps/app_cronometro.h,
// implemente a classe e acrescente uma linha abaixo. Veja placa/README.md.
#pragma once

#include "apps/app_avisos.h"
#include "apps/app_cartao.h"
#include "apps/app_codex.h"
#include "apps/app_configuracoes.h"
#include "apps/app_contagem.h"
#include "apps/app_conversas.h"
#include "apps/app_cronometro.h"
#include "apps/app_memoria.h"
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

inline void RegistrarApps(GavetaWatcher& gaveta) {
    DiagnosticoWatcher::Iniciar();  // motivo do último reinício e rastro anterior
    gaveta.Registrar(std::make_unique<AppSessoes>());
    gaveta.Registrar(std::make_unique<AppCodex>());
    gaveta.Registrar(std::make_unique<AppAvisos>());
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

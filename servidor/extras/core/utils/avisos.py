"""Frases amigáveis mostradas na tela do Watcher enquanto uma ferramenta roda."""

AVISOS = {
    "sessoes_listar": "Verificando suas sessões…",
    "sessao_ler": "Lendo a sessão…",
    "sessao_instruir": "Enviando a instrução…",
    "sessao_responder": "Respondendo na sessão…",
    "claude_perguntar": "Perguntando ao Claude…",
    "codex_perguntar": "Perguntando ao Codex…",
    "claude_nova_sessao": "Abrindo uma sessão do Claude…",
    "codex_nova_sessao": "Abrindo uma sessão do Codex…",
    "tarefa_resultado": "Buscando a resposta…",
    "projetos_listar": "Procurando o projeto…",
    "multica_status": "Consultando o Multica…",
    "multica_issues": "Consultando as issues…",
    "multica_agentes": "Consultando os agentes…",
    "multica_criar_issue": "Criando a issue…",
    "multica_comentar": "Comentando na issue…",
    "multica_autopilot": "Consultando os autopilots…",
    "mac_status": "Checando o Mac…",
    "mac_atalhos_listar": "Vendo os atalhos do Mac…",
    "mac_atalho_executar": "Executando o atalho…",
    "mac_notificar": "Avisando no Mac…",
    "previsao_tempo": "Vendo a previsão do tempo…",
    "self_camera_take_photo": "Olhando pela câmera…",
    "self_audio_speaker_set_volume": "Ajustando o volume…",
    "self_screen_set_brightness": "Ajustando o brilho…",
    "self_screen_set_theme": "Mudando o tema…",
    "lembrete_salvar": "Guardando o lembrete…",
    "lembretes_listar": "Vendo seus lembretes…",
    "lembrete_apagar": "Apagando o lembrete…",
    "uso_claude": "Vendo seu uso do Claude…",
    "handle_exit_intent": "",
}


def aviso_ferramenta(nome: str) -> str:
    return AVISOS.get(nome, "Um momento…")

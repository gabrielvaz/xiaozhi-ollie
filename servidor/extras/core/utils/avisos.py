"""Frases amigáveis mostradas na tela do Watcher enquanto uma ferramenta roda."""

from core.utils.idioma import t

AVISOS = {
    "sessoes_listar": t("Verificando suas sessões…", "Checking your sessions…", "正在查看你的会话…", "Revisando tus sesiones…"),
    "sessao_ler": t("Lendo a sessão…", "Reading the session…", "正在读取会话…", "Leyendo la sesión…"),
    "sessao_instruir": t("Enviando a instrução…", "Sending the instruction…", "正在发送指令…", "Enviando la instrucción…"),
    "sessao_responder": t("Respondendo na sessão…", "Replying in the session…", "正在会话中回复…", "Respondiendo en la sesión…"),
    "claude_perguntar": t("Perguntando ao Claude…", "Asking Claude…", "正在询问 Claude…", "Preguntando a Claude…"),
    "codex_perguntar": t("Perguntando ao Codex…", "Asking Codex…", "正在询问 Codex…", "Preguntando a Codex…"),
    "claude_nova_sessao": t("Abrindo uma sessão do Claude…", "Opening a Claude session…", "正在打开 Claude 会话…", "Abriendo una sesión de Claude…"),
    "codex_nova_sessao": t("Abrindo uma sessão do Codex…", "Opening a Codex session…", "正在打开 Codex 会话…", "Abriendo una sesión de Codex…"),
    "tarefa_resultado": t("Buscando a resposta…", "Fetching the answer…", "正在获取回复…", "Buscando la respuesta…"),
    "projetos_listar": t("Procurando o projeto…", "Looking for the project…", "正在查找项目…", "Buscando el proyecto…"),
    "multica_status": t("Consultando o Multica…", "Checking Multica…", "正在查询 Multica…", "Consultando Multica…"),
    "multica_issues": t("Consultando as issues…", "Checking the issues…", "正在查询 issues…", "Consultando las issues…"),
    "multica_agentes": t("Consultando os agentes…", "Checking the agents…", "正在查询智能体…", "Consultando los agentes…"),
    "multica_criar_issue": t("Criando a issue…", "Creating the issue…", "正在创建 issue…", "Creando la issue…"),
    "multica_comentar": t("Comentando na issue…", "Commenting on the issue…", "正在评论 issue…", "Comentando en la issue…"),
    "multica_mudar_status": t("Mudando a issue…", "Updating the issue…", "正在更新 issue…", "Cambiando la issue…"),
    "multica_daemon": t("Mexendo no daemon do Multica…", "Switching the Multica daemon…", "正在切换 Multica 守护进程…", "Cambiando el daemon de Multica…"),
    "multica_autopilot": t("Consultando os autopilots…", "Checking the autopilots…", "正在查询 autopilot…", "Consultando los autopilots…"),
    "mac_status": t("Checando o Mac…", "Checking the Mac…", "正在检查 Mac…", "Revisando el Mac…"),
    "mac_atalhos_listar": t("Vendo os atalhos do Mac…", "Looking at the Mac shortcuts…", "正在查看 Mac 快捷指令…", "Viendo los atajos del Mac…"),
    "mac_atalho_executar": t("Executando o atalho…", "Running the shortcut…", "正在运行快捷指令…", "Ejecutando el atajo…"),
    "mac_notificar": t("Avisando no Mac…", "Notifying on the Mac…", "正在 Mac 上通知…", "Avisando en el Mac…"),
    "previsao_tempo": t("Vendo a previsão do tempo…", "Checking the weather…", "正在查看天气预报…", "Viendo el pronóstico del tiempo…"),
    "self_camera_take_photo": t("Olhando pela câmera…", "Looking through the camera…", "正在通过摄像头查看…", "Mirando por la cámara…"),
    "self_audio_speaker_set_volume": t("Ajustando o volume…", "Adjusting the volume…", "正在调节音量…", "Ajustando el volumen…"),
    "self_screen_set_brightness": t("Ajustando o brilho…", "Adjusting the brightness…", "正在调节亮度…", "Ajustando el brillo…"),
    "self_screen_set_theme": t("Mudando o tema…", "Changing the theme…", "正在切换主题…", "Cambiando el tema…"),
    "lembrete_salvar": t("Guardando o lembrete…", "Saving the reminder…", "正在保存提醒…", "Guardando el recordatorio…"),
    "lembretes_listar": t("Vendo seus lembretes…", "Checking your reminders…", "正在查看你的提醒…", "Viendo tus recordatorios…"),
    "lembrete_apagar": t("Apagando o lembrete…", "Deleting the reminder…", "正在删除提醒…", "Borrando el recordatorio…"),
    "uso_claude": t("Vendo seu uso do Claude…", "Checking your Claude usage…", "正在查看你的 Claude 用量…", "Viendo tu uso de Claude…"),
    "handle_exit_intent": "",
}


def aviso_ferramenta(nome: str) -> str:
    return AVISOS.get(nome, t("Um momento…", "One moment…", "请稍等…", "Un momento…"))

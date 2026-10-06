"""Poses do Clawd escolhidas pelo servidor.

1. Emoji da resposta: o LLM abre a resposta com um emoji (ver agent-base-prompt-ptbr.txt) e o
   textUtils.get_emotion o troca pelo nome da animação. EMOJIS completa o EMOJI_MAP original
   (as 21 emoções do XiaoZhi) com as poses extras do mascote. O emoji não vai para a voz nem
   para a tela (textUtils.check_emoji).
2. Ferramenta em uso: enquanto uma ferramenta roda, o Watcher mostra uma pose que combina com ela
   (lendo sessões: codando; buscando conversas: lupa...).

Os nomes são os GIFs de firmwares/xiaozhi-ollie/mascote-clawd/emocoes.
Só emojis de um código, sem seletor de variação: o EMOJI_MAP compara caractere a caractere.
"""

import asyncio
import json

# emoji -> pose extra do Clawd
EMOJIS = {
    "👋": "waving",
    "🤓": "nerd",
    "📖": "reading",
    "💻": "codando",
    "📝": "teclando",
    "🏃": "running",
    "☕": "coffee",
    "🎉": "party",
    "🥳": "celebrating",
    "🎵": "music",
    "🔍": "searching",
    "💡": "idea",
    "🚀": "rocket",
    "🐛": "bug",
    "🌞": "sunny",
    "☔": "rainy",
    "🎤": "recording",
    "🛹": "skate",
    "😤": "chateado",
}

# Ferramenta -> pose enquanto ela roda. Vale o nome exato; depois o prefixo; senão "thinking".
_POR_NOME = {
    "sessoes_listar": "codando",
    "sessao_ler": "reading",
    "sessao_pergunta": "reading",
    "sessao_instruir": "teclando",
    "sessao_responder": "teclando",
    "sessao_escolher": "teclando",
    "codex_enviar": "teclando",
    "tarefa_resultado": "searching",
    "claude_nova_sessao": "rocket",
    "codex_nova_sessao": "rocket",
    "claude_perguntar": "nerd",
    "codex_perguntar": "nerd",
    "projetos_listar": "searching",
    "conversas_buscar": "searching",
    "multica_issues": "searching",
    "multica_criar_issue": "teclando",
    "multica_comentar": "teclando",
    "multica_autopilot": "rocket",
    "uso_claude": "nerd",
    "lembrete_salvar": "idea",
    "lembretes_listar": "reading",
    "previsao_tempo": "sunny",
    "handle_exit_intent": "waving",
}
_POR_PREFIXO = (
    ("multica_", "working"),
    ("mac_", "working"),
    ("lembrete", "reading"),
    ("reuniao", "recording"),
    ("self_camera", "searching"),
    ("self_", ""),          # ajustes do próprio aparelho (volume, brilho, abrir app): sem pose
)


def emocao_da_ferramenta(nome: str) -> str:
    if nome in _POR_NOME:
        return _POR_NOME[nome]
    for prefixo, emocao in _POR_PREFIXO:
        if nome.startswith(prefixo):
            return emocao
    return "thinking"


def mostrar_ferramenta(conn, nome: str) -> None:
    """Manda a pose da ferramenta ao Watcher (chamado da thread do LLM)."""
    emocao = emocao_da_ferramenta(nome or "")
    if not emocao:
        return
    mensagem = json.dumps({"type": "llm", "text": "", "emotion": emocao, "session_id": conn.session_id})
    try:
        asyncio.run_coroutine_threadsafe(conn.websocket.send(mensagem), conn.loop)
    except Exception:
        pass

"""Idioma do Ollie no servidor: IDIOMA no .env (pt-BR, en-US, zh-CN ou es-ES), o mesmo do firmware.

t("português", "English", "中文", "español") escolhe o texto fixo que vai para a tela ou para a voz.
NOME é o nome do idioma para os prompts ("Responda sempre em ..."); o modelo responde nele.
"""

import os

IDIOMAS = ("pt-BR", "en-US", "zh-CN", "es-ES")

IDIOMA = os.environ.get("IDIOMA", "pt-BR").strip() or "pt-BR"
if IDIOMA not in IDIOMAS:
    IDIOMA = "pt-BR"

NOMES = {
    "pt-BR": "português do Brasil",
    "en-US": "American English",
    "zh-CN": "简体中文 (Simplified Chinese)",
    "es-ES": "español de España",
}
NOME = NOMES[IDIOMA]

# Vozes padrão (nomes no formato Azure Neural, aceitos pelo MAI Voice); VOZ no .env tem prioridade
VOZES = {
    "pt-BR": "pt-BR-FranciscaNeural",
    "en-US": "en-US-JennyNeural",
    "zh-CN": "zh-CN-XiaoxiaoNeural",
    "es-ES": "es-ES-ElviraNeural",
}

# Código curto para APIs de transcrição (parâmetro "language")
CODIGO = IDIOMA.split("-")[0]


def t(pt: str, en: str, zh: str, es: str) -> str:
    return {"pt-BR": pt, "en-US": en, "zh-CN": zh, "es-ES": es}[IDIOMA]

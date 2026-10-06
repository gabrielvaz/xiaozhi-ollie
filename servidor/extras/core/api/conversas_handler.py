"""Rotas do app Conversas do Watcher (mesmo token do WebSocket):

GET /watcher/perfil?agente=Nome        grava o nome do agente e devolve {usuario, agente}
GET /watcher/conversas                 últimas conversas: [{id, titulo, detalhe}]
GET /watcher/conversas/{id}            {titulo, texto} com o que foi dito (sem ferramentas)
GET /watcher/conversas/{id}/audio      .ogg com a conversa narrada na voz do Ollie (gerado na hora, depois em cache)
"""

import asyncio
import hashlib
import os
import re
import subprocess
from datetime import datetime

from aiohttp import web

from core.api.avisos_handler import AvisosHandler
from core.utils import diario, memoria, perfil
from core.utils.idioma import t
from core.utils.vigia import rotulo_dia

ID_VALIDO = re.compile(r"^\d{8}-\d{6}$")
LIMITE_AUDIO = 6000   # caracteres narrados (~6 min de áudio)


def _detalhe(c: dict) -> str:
    quando = datetime.fromisoformat(c["inicio"])
    return f"{rotulo_dia(quando)} {quando:%H:%M} · {c.get('falas', 0)} " + t("falas", "messages", "条对话", "mensajes")


def _texto(c: dict) -> str:
    partes = []
    if c.get("resumo"):
        partes.append(t("Resumo", "Summary", "摘要", "Resumen") + f": {c['resumo']}\n")
    voce = t("Você", "You", "你", "Tú")
    partes += [f"{voce if quem == 'Você' else quem}: {texto}" for quem, texto in diario.falas(c)]
    return "\n".join(partes)


def _audio(c: dict) -> str:
    """Narra a conversa em partes (o TTS aceita textos curtos) e junta num único .ogg."""
    roteiro = []
    voce_disse = t("Você disse", "You said", "你说", "Dijiste")
    ollie_respondeu = t("Ollie respondeu", "Ollie replied", "Ollie 回答", "Ollie respondió")
    for quem, texto in diario.falas(c):  # diario.falas devolve sempre "Você" para o usuário
        roteiro.append(f"{voce_disse}: {texto}" if quem == "Você" else f"{ollie_respondeu}: {texto}")
    corrido = " ".join(roteiro)[:LIMITE_AUDIO] or t("Esta conversa não tem falas.", "This conversation has no messages.",
                                                    "这段对话没有内容。", "Esta conversación no tiene mensajes.")
    nome = "conversa-" + hashlib.sha1(corrido.encode("utf-8")).hexdigest()[:12] + ".ogg"
    destino = memoria.CACHE / nome
    if destino.exists():
        return nome
    blocos, atual = [], ""
    for frase in re.split(r"(?<=[.!?])\s+|(?<=[。！？])", corrido):
        if len(atual) + len(frase) > 1200 and atual:
            blocos.append(atual)
            atual = ""
        atual = f"{atual} {frase}".strip()
    if atual:
        blocos.append(atual)
    memoria.CACHE.mkdir(parents=True, exist_ok=True)
    mp3 = memoria.CACHE / (nome + ".mp3")
    with open(mp3, "wb") as f:
        for bloco in blocos:
            r = memoria._api("/audio/speech", json={"model": os.environ.get("MODELO_TTS"),
                                                    "voice": os.environ.get("VOZ"),
                                                    "response_format": "mp3", "input": bloco})
            r.raise_for_status()
            f.write(r.content)
    subprocess.run([memoria.FFMPEG, "-y", "-loglevel", "error", "-i", str(mp3), "-ar", "16000", "-ac", "1",
                    "-c:a", "libopus", "-b:a", "24k", "-frame_duration", "60", "-application", "voip",
                    str(destino)], check=True)
    mp3.unlink(missing_ok=True)
    return nome


class ConversasHandler(AvisosHandler):
    def __init__(self, config):
        super().__init__(config)
        diario.iniciar()  # migra o formato antigo e resume as conversas encerradas

    async def handle_perfil(self, request: web.Request) -> web.Response:
        if not self._autorizado(request):
            return web.json_response({"erro": "não autorizado"}, status=401)
        if request.query.get("agente"):
            perfil.definir_agente(request.query["agente"])
        return web.json_response({"usuario": perfil.usuario(), "agente": perfil.nome_agente()})

    async def handle_lista(self, request: web.Request) -> web.Response:
        if not self._autorizado(request):
            return web.json_response({"erro": "não autorizado"}, status=401)
        itens = await asyncio.to_thread(diario.conversas, 40)
        return web.json_response({"conversas": [
            {"id": c["id"], "titulo": c.get("titulo") or t("Conversa em andamento", "Conversation in progress",
                                                                     "对话进行中", "Conversación en curso"), "detalhe": _detalhe(c)}
            for c in itens]})

    def _buscar(self, request: web.Request) -> dict | None:
        cid = request.match_info["id"]
        return diario.conversa(cid) if ID_VALIDO.match(cid) else None

    async def handle_conversa(self, request: web.Request) -> web.Response:
        if not self._autorizado(request):
            return web.json_response({"erro": "não autorizado"}, status=401)
        c = self._buscar(request)
        if c is None:
            return web.json_response({"erro": "não encontrada"}, status=404)
        texto = await asyncio.to_thread(_texto, c)
        return web.json_response({"titulo": c.get("titulo") or t("Conversa", "Conversation", "对话", "Conversación"), "texto": texto[:8000]})

    async def handle_audio(self, request: web.Request) -> web.StreamResponse:
        if not self._autorizado(request):
            return web.json_response({"erro": "não autorizado"}, status=401)
        c = self._buscar(request)
        if c is None:
            return web.json_response({"erro": "não encontrada"}, status=404)
        try:
            nome = await asyncio.to_thread(_audio, c)
        except Exception as e:
            return web.json_response({"erro": f"falha ao gerar o áudio: {e}"}, status=502)
        return web.FileResponse(memoria.CACHE / nome, headers={"Content-Type": "audio/ogg"})

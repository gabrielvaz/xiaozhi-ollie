"""Rotas do microSD do Watcher (mesmo token do WebSocket):

POST /watcher/upload?tipo=conversas|reunioes&nome=ARQ&offset=N[&fim=1]  envio em partes
GET  /watcher/memoria                                                   itens da memória offline
GET  /watcher/memoria/audio/{nome}                                      áudio .ogg de um item
"""

import asyncio
import re
from pathlib import Path

from aiohttp import web

from core.api.avisos_handler import AvisosHandler
from core.utils import memoria
from core.utils.reuniao import processar_backup
from core.utils.vigia import ponte_carregada

DESTINO = Path.home() / "Library/Mobile Documents/com~apple~CloudDocs/Watcher/Do cartão"
NOME_VALIDO = re.compile(r"^[A-Za-z0-9._-]{1,64}$")


class CartaoHandler(AvisosHandler):
    async def handle_upload(self, request: web.Request) -> web.Response:
        if not self._autorizado(request):
            return web.json_response({"erro": "não autorizado"}, status=401)
        tipo, nome = request.query.get("tipo", ""), request.query.get("nome", "")
        if tipo not in ("conversas", "reunioes") or not NOME_VALIDO.match(nome):
            return web.json_response({"erro": "parâmetros inválidos"}, status=400)
        offset = int(request.query.get("offset", "0"))
        pasta = DESTINO / tipo
        pasta.mkdir(parents=True, exist_ok=True)
        arq = pasta / nome
        dados = await request.read()
        tamanho_atual = arq.stat().st_size if arq.exists() else 0
        if offset != tamanho_atual and offset != 0:
            return web.json_response({"erro": "offset fora de ordem", "tamanho": tamanho_atual}, status=409)
        with open(arq, "r+b" if (arq.exists() and offset) else "wb") as f:
            f.seek(offset)
            f.write(dados)
            f.truncate()
        resultado = "ok"
        if request.query.get("fim") == "1" and tipo == "reunioes":
            resultado = await asyncio.to_thread(processar_backup, arq)
        return web.json_response({"tamanho": arq.stat().st_size, "resultado": resultado})

    async def handle_memoria(self, request: web.Request) -> web.Response:
        if not self._autorizado(request):
            return web.json_response({"erro": "não autorizado"}, status=401)
        itens = await asyncio.to_thread(memoria.gerar, ponte_carregada())
        return web.json_response({"itens": itens})

    async def handle_audio(self, request: web.Request) -> web.StreamResponse:
        if not self._autorizado(request):
            return web.json_response({"erro": "não autorizado"}, status=401)
        arq = memoria.caminho_audio(request.match_info["nome"])
        if arq is None:
            return web.json_response({"erro": "não encontrado"}, status=404)
        return web.FileResponse(arq, headers={"Content-Type": "audio/ogg"})

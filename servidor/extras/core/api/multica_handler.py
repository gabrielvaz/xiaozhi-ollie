"""App "Multica" do Watcher: ver e controlar o Multica deste Mac (mesmo token do WebSocket).

GET  /watcher/multica                          -> {"daemon": {"ligado", "detalhe"}, "issues": [{"id", "titulo", "detalhe", "status"}],
                                                   "autopilots": [{"id", "titulo", "detalhe"}], "erro": str | null}
GET  /watcher/multica/issues/{id}              -> {"id", "titulo", "status", "texto"}
POST /watcher/multica/issues/{id}/status       {"status": "todo"|"in_progress"|...} -> {"ok", "mensagem"}
POST /watcher/multica/daemon                   {"ligar": bool} -> {"ok", "mensagem"}
POST /watcher/multica/autopilots/{id}/disparar -> {"ok", "mensagem"}

O aparelho manda o corpo como application/octet-stream: ele é lido e interpretado como JSON.
As ações só chegam aqui depois de uma tela de confirmação no aparelho.
"""

import asyncio
import json
from urllib.parse import unquote

from aiohttp import web

from core.api.avisos_handler import AvisosHandler
from core.utils import multica_remoto


async def _corpo(request: web.Request) -> dict | None:
    try:
        dados = json.loads((await request.read()).decode("utf-8") or "{}")
    except (ValueError, UnicodeDecodeError):
        return None
    return dados if isinstance(dados, dict) else None


def _id(request: web.Request) -> str:
    return unquote(request.match_info.get("id", "")).strip()


def _negado() -> web.Response:
    return web.json_response({"erro": "não autorizado"}, status=401)


class MulticaHandler(AvisosHandler):
    async def handle_resumo(self, request: web.Request) -> web.Response:
        if not self._autorizado(request):
            return _negado()
        return web.json_response(await asyncio.to_thread(multica_remoto.resumo))

    async def handle_issue(self, request: web.Request) -> web.Response:
        if not self._autorizado(request):
            return _negado()
        dados = await asyncio.to_thread(multica_remoto.ler_issue, _id(request))
        if dados is None:
            return web.json_response({"erro": "issue não encontrada"}, status=404)
        return web.json_response(dados)

    async def handle_status(self, request: web.Request) -> web.Response:
        if not self._autorizado(request):
            return _negado()
        corpo = await _corpo(request)
        if corpo is None or not isinstance(corpo.get("status"), str):
            return web.json_response({"ok": False, "mensagem": 'Corpo inválido: use {"status": "done"}.'}, status=400)
        return web.json_response(await asyncio.to_thread(multica_remoto.mudar_situacao, _id(request), corpo["status"]))

    async def handle_daemon(self, request: web.Request) -> web.Response:
        if not self._autorizado(request):
            return _negado()
        corpo = await _corpo(request)
        if corpo is None or not isinstance(corpo.get("ligar"), bool):
            return web.json_response({"ok": False, "mensagem": 'Corpo inválido: use {"ligar": true}.'}, status=400)
        return web.json_response(await asyncio.to_thread(multica_remoto.definir_daemon, corpo["ligar"]))

    async def handle_disparar(self, request: web.Request) -> web.Response:
        if not self._autorizado(request):
            return _negado()
        return web.json_response(await asyncio.to_thread(multica_remoto.disparar_autopilot, _id(request)))

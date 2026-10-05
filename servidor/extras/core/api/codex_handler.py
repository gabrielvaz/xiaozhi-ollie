"""App "Codex" do Watcher: ver e controlar o Codex (OpenAI Codex CLI) deste Mac (mesmo token do WebSocket).

GET  /watcher/codex                       -> {"remoto": {"ligado", "detalhe"}, "sessoes": [{"id", "titulo", "detalhe"}],
                                              "nuvem": [{"id", "titulo", "detalhe"}], "erro_nuvem": str | null}
GET  /watcher/codex/sessoes/{id}          -> {"titulo", "situacao", "mensagens": [{"quem": "Você"|"Codex", "hora", "texto"}]}
POST /watcher/codex/sessoes/{id}/enviar   {"texto", "modo"?: "auto"|"fila"|"executar", "escrita"?, "confirmado"?}
                                          -> {"ok", "mensagem", "precisa_confirmar"?}
POST /watcher/codex/nova                  {"texto", "pasta"?, "escrita"?, "confirmado"?} -> {"ok", "mensagem", "id"?}
POST /watcher/codex/remoto                {"ligar": bool, "confirmado"?} -> {"ok", "mensagem", "precisa_confirmar"?}
GET  /watcher/codex/nuvem/{id}            -> {"titulo", "texto"}

O aparelho manda o corpo como application/octet-stream: ele é lido e interpretado como JSON.
Por padrão tudo roda em somente leitura; alterar arquivos ou ligar o controle remoto exige "confirmado": true.
"""

import asyncio
import json
from urllib.parse import unquote

from aiohttp import web

from core.api.avisos_handler import AvisosHandler
from core.utils import codex_remoto


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


class CodexHandler(AvisosHandler):
    async def handle_resumo(self, request: web.Request) -> web.Response:
        if not self._autorizado(request):
            return _negado()
        remoto, sessoes, (nuvem, erro_nuvem) = await asyncio.gather(
            asyncio.to_thread(codex_remoto.status_remoto),
            asyncio.to_thread(codex_remoto.listar_sessoes),
            asyncio.to_thread(codex_remoto.listar_nuvem))
        return web.json_response({"remoto": remoto, "sessoes": sessoes, "nuvem": nuvem, "erro_nuvem": erro_nuvem})

    async def handle_sessao(self, request: web.Request) -> web.Response:
        if not self._autorizado(request):
            return _negado()
        dados = await asyncio.to_thread(codex_remoto.ler_sessao, _id(request))
        if dados is None:
            return web.json_response({"erro": "sessão não encontrada"}, status=404)
        return web.json_response(dados)

    async def handle_enviar(self, request: web.Request) -> web.Response:
        if not self._autorizado(request):
            return _negado()
        corpo = await _corpo(request)
        if corpo is None or not isinstance(corpo.get("texto"), str):
            return web.json_response({"ok": False, "mensagem": 'Corpo inválido: use {"texto": "..."}.'}, status=400)
        r = await asyncio.to_thread(codex_remoto.enviar, _id(request), corpo["texto"],
                                    str(corpo.get("modo") or "auto"), bool(corpo.get("escrita")),
                                    corpo.get("confirmado") is True)
        return web.json_response(r)

    async def handle_nova(self, request: web.Request) -> web.Response:
        if not self._autorizado(request):
            return _negado()
        corpo = await _corpo(request)
        if corpo is None or not isinstance(corpo.get("texto"), str):
            return web.json_response({"ok": False, "mensagem": 'Corpo inválido: use {"texto": "..."}.'}, status=400)
        pasta = corpo.get("pasta") if isinstance(corpo.get("pasta"), str) else None
        r = await asyncio.to_thread(codex_remoto.iniciar_tarefa, corpo["texto"], pasta,
                                    bool(corpo.get("escrita")), corpo.get("confirmado") is True)
        return web.json_response(r)

    async def handle_remoto(self, request: web.Request) -> web.Response:
        if not self._autorizado(request):
            return _negado()
        corpo = await _corpo(request)
        if corpo is None or not isinstance(corpo.get("ligar"), bool):
            return web.json_response({"ok": False, "mensagem": 'Corpo inválido: use {"ligar": true}.'}, status=400)
        r = await asyncio.to_thread(codex_remoto.definir_remoto, corpo["ligar"], corpo.get("confirmado") is True)
        return web.json_response(r)

    async def handle_nuvem(self, request: web.Request) -> web.Response:
        if not self._autorizado(request):
            return _negado()
        dados = await asyncio.to_thread(codex_remoto.ler_nuvem, _id(request))
        if "erro" in dados:
            return web.json_response(dados, status=404 if "não encontrada" in dados["erro"] else 502)
        return web.json_response(dados)

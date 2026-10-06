"""Rotas do microSD do Watcher (mesmo token do WebSocket):

POST /watcher/upload?tipo=conversas|reunioes&nome=ARQ&offset=N[&fim=1]  envio em partes
POST /watcher/backup                                                    copia as conversas para o iCloud
POST /watcher/diagnostico                                               pulso por minuto e relatório de reinício
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

    _ultimo_pulso: dict = {}

    async def handle_diagnostico(self, request: web.Request) -> web.Response:
        """Grava em data/diagnostico.log (uma linha JSON por evento) os pulsos e reinícios anormais do Watcher,
        e marca os buracos entre pulsos: aparelho congelado, sem rede ou desligado."""
        if not self._autorizado(request):
            return web.json_response({"erro": "não autorizado"}, status=401)
        import json
        import time
        from datetime import datetime
        try:
            dados = json.loads((await request.read()).decode("utf-8", "replace"))
        except ValueError:
            return web.json_response({"erro": "corpo inválido"}, status=400)
        agora = time.time()
        aparelho = request.headers.get("Device-Id", "?")
        linhas = []
        anterior = self._ultimo_pulso.get(aparelho)
        if dados.get("tipo") == "pulso" and anterior:
            intervalo = agora - anterior["quando"]
            if intervalo > 150:
                reiniciou = dados.get("ligado_s", 0) < anterior.get("ligado_s", 0)
                linhas.append({"tipo": "buraco", "sem_pulso_s": int(intervalo),
                               "explicacao": "reiniciou nesse meio-tempo" if reiniciou
                               else "não reiniciou: congelado, sem rede ou com a gaveta presa"})
        if dados.get("tipo") == "pulso":
            self._ultimo_pulso[aparelho] = {"quando": agora, "ligado_s": dados.get("ligado_s", 0)}
        linhas.append(dados)
        caminho = Path(__file__).resolve().parents[2] / "data/diagnostico.log"
        with open(caminho, "a", encoding="utf-8") as f:
            for linha in linhas:
                f.write(json.dumps({"quando": datetime.now().isoformat(timespec="seconds"), "aparelho": aparelho,
                                    **linha}, ensure_ascii=False) + "\n")
        if dados.get("tipo") == "reinicio":
            import logging
            logging.getLogger(__name__).warning(f"Watcher reiniciou: {dados.get('motivo')}")
        return web.json_response({"ok": True})

    async def handle_backup(self, request: web.Request) -> web.Response:
        """App Backup do Watcher: o único momento em que as conversas são copiadas para o iCloud."""
        if not self._autorizado(request):
            return web.json_response({"erro": "não autorizado"}, status=401)
        from core.utils import diario
        try:
            copiados = await asyncio.to_thread(diario.espelhar)
        except OSError as e:
            return web.json_response({"ok": False, "copiados": 0, "erro": str(e)[:200]})
        return web.json_response({"ok": True, "copiados": copiados})

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

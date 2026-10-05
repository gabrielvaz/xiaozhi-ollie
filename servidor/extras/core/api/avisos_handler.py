"""GET /watcher/avisos?desde=N — avisos para a tela do Watcher (mesmo token do WebSocket)."""

from aiohttp import web

from core.auth import AuthManager
from core.utils.vigia import avisos_desde


class AvisosHandler:
    def __init__(self, config: dict):
        self.auth = AuthManager(secret_key=config["server"]["auth_key"],
                                expire_seconds=config["server"].get("auth", {}).get("expire_seconds"))

    def _autorizado(self, request: web.Request) -> bool:
        token = request.headers.get("Authorization", "").removeprefix("Bearer ").strip()
        return self.auth.verify_token(token, client_id=request.headers.get("Client-Id", ""),
                                      username=request.headers.get("Device-Id", ""))

    async def handle_sessoes(self, request: web.Request) -> web.Response:
        """Lista compacta para o navegador de sessões do Watcher (só Claude Code e Codex não parados há muito)."""
        if not self._autorizado(request):
            return web.json_response({"erro": "não autorizado"}, status=401)
        import asyncio
        from core.utils.vigia import ponte_carregada
        ponte = ponte_carregada()
        agentes = await asyncio.to_thread(ponte._agentes)
        rotulos = {"esperando você": "Esperando você", "trabalhando": "Trabalhando", "subagentes rodando": "Subagentes",
                   "concluiu recentemente": "Concluída", "parada": "Parada"}
        sessoes = []
        for a in agentes:
            if a["situacao"] == "parada" and len(sessoes) >= 12:
                continue
            minutos = a.get("minutos_desde_ultima_atividade")
            sessoes.append({
                "id": a["sessao"],
                "titulo": (a.get("titulo") or a.get("workspace") or a.get("pasta") or "Sessão")[:40],
                "agente": a.get("agente", ""),
                "situacao": rotulos.get(a["situacao"], a["situacao"]),
                "ha": "" if minutos is None else (f"{minutos} min" if minutos < 60 else f"{minutos // 60} h"),
                "ultima": (a.get("ultima_fala") or "")[:300],
            })
        return web.json_response({"sessoes": sessoes[:20]})

    async def handle_mensagens(self, request: web.Request) -> web.Response:
        """Últimas mensagens (você e o agente) de uma sessão, da mais antiga para a mais nova."""
        if not self._autorizado(request):
            return web.json_response({"erro": "não autorizado"}, status=401)
        import asyncio
        from urllib.parse import unquote
        from core.utils.vigia import ponte_carregada
        sessao = unquote(request.match_info.get("id", ""))
        ponte = ponte_carregada()
        rotulos = {"esperando você": "Esperando você", "trabalhando": "Trabalhando", "subagentes rodando": "Subagentes",
                   "concluiu recentemente": "Concluída", "parada": "Parada"}

        def ler():
            agente = next((a for a in ponte._agentes() if a["sessao"] == sessao), None)
            if agente is None:
                return None
            msgs = None
            if agente.get("agente") == "claude":
                try:
                    msgs = ponte.mensagens_claude(agente.get("_sessao_id", ""), agente.get("_cwd", ""))
                except OSError:
                    msgs = None
            if not msgs:
                msgs = ponte.mensagens_tela(sessao, agente.get("agente") or "")
            while len(msgs) > 1 and sum(len(m["texto"]) for m in msgs) > 5000:
                msgs.pop(0)
            return {
                "titulo": (agente.get("titulo") or agente.get("workspace") or agente.get("pasta") or "Sessão")[:40],
                "situacao": rotulos.get(agente["situacao"], agente["situacao"]),
                "mensagens": msgs,
            }

        dados = await asyncio.to_thread(ler)
        if dados is None:
            return web.json_response({"erro": "sessão não encontrada"}, status=404)
        return web.json_response(dados)

    async def handle_pergunta(self, request: web.Request) -> web.Response:
        """Pergunta com opções aberta na tela da sessão (AskUserQuestion ou permissão): {"pergunta": {...} | null}."""
        if not self._autorizado(request):
            return web.json_response({"erro": "não autorizado"}, status=401)
        import asyncio
        from urllib.parse import unquote
        from core.utils.vigia import ponte_carregada
        sessao = unquote(request.match_info.get("id", ""))
        return web.json_response({"pergunta": await asyncio.to_thread(ponte_carregada().pergunta_tela, sessao)})

    async def handle_responder(self, request: web.Request) -> web.Response:
        """POST {"escolhas": [1, 3]}: marca as opções na pergunta aberta e confirma (o corpo pode vir como octet-stream)."""
        if not self._autorizado(request):
            return web.json_response({"erro": "não autorizado"}, status=401)
        import asyncio
        import json
        from urllib.parse import unquote
        from core.utils.vigia import ponte_carregada
        sessao = unquote(request.match_info.get("id", ""))
        try:
            corpo = json.loads((await request.read()).decode("utf-8") or "{}")
            escolhas = corpo.get("escolhas")
            if isinstance(escolhas, int):
                escolhas = [escolhas]
            if not isinstance(escolhas, list):
                raise ValueError
        except (ValueError, AttributeError, UnicodeDecodeError):
            return web.json_response({"ok": False, "mensagem": 'Corpo inválido: use {"escolhas": [1]}.'}, status=400)
        return web.json_response(await asyncio.to_thread(ponte_carregada().responder_pergunta, sessao, escolhas))

    async def handle_uso(self, request: web.Request) -> web.Response:
        """Limites do Claude (5 h e 7 dias) lidos no Mac; o token não vai para o aparelho."""
        if not self._autorizado(request):
            return web.json_response({"erro": "não autorizado"}, status=401)
        import asyncio
        from core.utils.vigia import ponte_carregada
        return web.json_response(await asyncio.to_thread(ponte_carregada().uso_claude_dados))

    async def handle_qrcodes(self, request: web.Request) -> web.Response:
        """QR codes do app "Mostrar QR": linhas "Título | conteúdo" em iCloud Drive/Watcher/QR.md."""
        if not self._autorizado(request):
            return web.json_response({"erro": "não autorizado"}, status=401)
        from pathlib import Path
        arq = Path.home() / "Library/Mobile Documents/com~apple~CloudDocs/Watcher/QR.md"
        if not arq.exists():
            arq.parent.mkdir(parents=True, exist_ok=True)
            arq.write_text("# QR codes do Watcher\n\nUma linha por QR: Título | conteúdo (link, texto, contato).\n\n"
                           "- Projeto no GitHub | https://github.com/gabrielvaz/xiaozhi-ollie\n", encoding="utf-8")
        itens = []
        from core.utils.icloud import ler_texto
        for linha in ler_texto(arq, padrao="").splitlines():
            if linha.startswith("- ") and "|" in linha:
                titulo, conteudo = (p.strip() for p in linha[2:].split("|", 1))
                if titulo and conteudo:
                    itens.append({"titulo": titulo[:40], "texto": conteudo[:300]})
        return web.json_response({"itens": itens[:20]})

    async def handle_tempo(self, request: web.Request) -> web.Response:
        """Previsão estruturada para a tela do Watcher (local pelo IP real que chega pelo Funnel)."""
        if not self._autorizado(request):
            return web.json_response({"erro": "não autorizado"}, status=401)
        import asyncio
        from plugins_func.functions.previsao_tempo import dados_tempo
        ip = (request.headers.get("x-forwarded-for") or request.remote or "").split(",")[0].strip()
        try:
            return web.json_response(await asyncio.to_thread(dados_tempo, ip))
        except Exception as e:
            return web.json_response({"ok": False, "erro": f"Previsão indisponível: {e}"})

    async def handle_historico(self, request: web.Request) -> web.Response:
        """GET /watcher/avisos/historico: todos os avisos guardados, mais recentes primeiro."""
        if not self._autorizado(request):
            return web.json_response({"erro": "não autorizado"}, status=401)
        from core.utils.vigia import historico
        return web.json_response({"avisos": historico()})

    async def handle_get(self, request: web.Request) -> web.Response:
        if not self._autorizado(request):
            return web.json_response({"erro": "não autorizado"}, status=401)
        try:
            desde = int(request.query.get("desde", "-1"))
        except ValueError:
            desde = -1
        return web.json_response(avisos_desde(desde))

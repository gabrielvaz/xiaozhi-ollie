"""GET /watcher/reunioes — reuniões gravadas (mesmo token do WebSocket).

Resposta: {"reunioes": [{"id": "2026-10-05 19h30", "titulo": "...", "detalhe": "Hoje 19:30 · 12 min"}]},
mais recentes primeiro, no máximo 30. Título = 1ª linha de resumo.md (ou "Processando…").
"""

import asyncio
import json
import subprocess
import wave
from datetime import datetime
from pathlib import Path

from aiohttp import web

from core.api.avisos_handler import AvisosHandler
from core.utils import reuniao
from core.utils.icloud import ler_texto

DIAS = ["Seg", "Ter", "Qua", "Qui", "Sex", "Sáb", "Dom"]
FFPROBE = "/opt/homebrew/bin/ffprobe"
LIMITE = 30


def _duracao_s(pasta: Path) -> int | None:
    info = ler_texto(pasta / "info.json", padrao="") if (pasta / "info.json").exists() else ""
    if info:
        try:
            return int(json.loads(info)["duracao_s"])
        except (ValueError, KeyError, TypeError):
            pass
    wav = pasta / "audio.wav"
    if wav.exists():
        try:
            with wave.open(str(wav), "rb") as w:
                if w.getnframes():
                    return int(w.getnframes() / w.getframerate())
        except (wave.Error, OSError, EOFError):
            pass
    m4a = pasta / "audio.m4a"
    if m4a.exists():
        try:
            r = subprocess.run([FFPROBE, "-v", "error", "-show_entries", "format=duration", "-of", "csv=p=0", str(m4a)],
                               capture_output=True, text=True, timeout=15)
            return int(float(r.stdout.strip()))
        except (OSError, ValueError, subprocess.SubprocessError):
            pass
    return None


def _formatar_duracao(s: int) -> str:
    if s < 60:
        return f"{s} s"
    minutos = round(s / 60)
    if minutos < 60:
        return f"{minutos} min"
    return f"{minutos // 60} h {minutos % 60:02d} min"


def _detalhe(quando: datetime, duracao: int | None) -> str:
    hoje = datetime.now().date()
    if quando.date() == hoje:
        dia = "Hoje"
    elif (hoje - quando.date()).days == 1:
        dia = "Ontem"
    else:
        dia = f"{DIAS[quando.weekday()]} {quando:%d/%m}"
    texto = f"{dia} {quando:%H:%M}"
    return texto if duracao is None else f"{texto} · {_formatar_duracao(duracao)}"


def listar() -> list[dict]:
    if not reuniao.PASTA.is_dir():
        return []
    pastas = []
    for p in reuniao.PASTA.iterdir():
        try:
            quando = datetime.strptime(p.name, "%Y-%m-%d %Hh%M")
        except ValueError:
            continue
        if p.is_dir():
            pastas.append((quando, p))
    itens = []
    for quando, p in sorted(pastas, reverse=True):
        resumo = p / "resumo.md"
        tem_audio = (p / "audio.wav").exists() or (p / "audio.m4a").exists()
        if not tem_audio and not resumo.exists():
            continue
        titulo = "Processando…"
        if resumo.exists():
            primeira = ler_texto(resumo, padrao="").strip().splitlines()
            if primeira:
                titulo = primeira[0].removeprefix("# ").strip() or titulo
        itens.append({"id": p.name, "titulo": titulo, "detalhe": _detalhe(quando, _duracao_s(p))})
        if len(itens) >= LIMITE:
            break
    return itens


class ReunioesHandler(AvisosHandler):
    async def handle_lista(self, request: web.Request) -> web.Response:
        if not self._autorizado(request):
            return web.json_response({"erro": "não autorizado"}, status=401)
        return web.json_response({"reunioes": await asyncio.to_thread(listar)})

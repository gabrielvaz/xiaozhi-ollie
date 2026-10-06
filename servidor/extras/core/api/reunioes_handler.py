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
from core.utils.idioma import t
from core.utils.vigia import rotulo_dia

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
    un_min = t("min", "min", "分钟", "min")
    if s < 60:
        return f"{s} " + t("s", "s", "秒", "s")
    minutos = round(s / 60)
    if minutos < 60:
        return f"{minutos} {un_min}"
    return f"{minutos // 60} " + t("h", "h", "小时", "h") + f" {minutos % 60:02d} {un_min}"


def _detalhe(quando: datetime, duracao: int | None) -> str:
    texto = f"{rotulo_dia(quando)} {quando:%H:%M}"
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
        titulo = t("Processando…", "Processing…", "处理中…", "Procesando…")
        if resumo.exists():
            primeira = ler_texto(resumo, padrao="").strip().splitlines()
            if primeira:
                titulo = primeira[0].removeprefix("# ").strip() or titulo
        itens.append({"id": p.name, "titulo": titulo, "detalhe": _detalhe(quando, _duracao_s(p))})
        if len(itens) >= LIMITE:
            break
    return itens


def detalhe(rid: str) -> dict | None:
    """Uma reunião: título, quando, duração e o resumo (sem os títulos de Markdown)."""
    p = reuniao.PASTA / rid
    try:
        quando = datetime.strptime(rid, "%Y-%m-%d %Hh%M")
    except ValueError:
        return None
    if not p.is_dir():
        return None
    linhas = ler_texto(p / "resumo.md", padrao="").strip().splitlines() if (p / "resumo.md").exists() else []
    titulo = linhas[0].removeprefix("# ").strip() if linhas else t("Processando…", "Processing…", "处理中…", "Procesando…")
    # Só a seção de resumo (as listas de decisões e próximos passos ficam no Notas e no arquivo)
    secao, dentro = [], False
    for l in linhas[1:]:
        if l.startswith("## "):
            if dentro:
                break
            dentro = True
            continue
        if dentro:
            secao.append(l)
    corpo = "\n".join(secao).strip() or "\n".join(l for l in linhas[1:] if not l.startswith("#")).strip()
    duracao = _duracao_s(p)
    cabecalho = f"{rotulo_dia(quando)} {quando:%H:%M}"
    if duracao is not None:
        cabecalho += " · " + t("duração", "duration", "时长", "duración") + " " + _formatar_duracao(duracao)
    if not corpo:
        corpo = t("Ainda processando: a transcrição e o resumo aparecem aqui em alguns minutos.",
                  "Still processing: the transcript and summary show up here in a few minutes.",
                  "仍在处理：几分钟后这里会显示转写和摘要。",
                  "Todavía procesando: la transcripción y el resumen aparecen aquí en unos minutos.")
    return {"titulo": titulo, "texto": f"{cabecalho}\n\n{corpo}"[:3000]}


class ReunioesHandler(AvisosHandler):
    async def handle_reuniao(self, request: web.Request) -> web.Response:
        if not self._autorizado(request):
            return web.json_response({"erro": "não autorizado"}, status=401)
        dados = await asyncio.to_thread(detalhe, request.match_info["id"])
        if dados is None:
            return web.json_response({"erro": "não encontrada"}, status=404)
        return web.json_response(dados)

    async def handle_lista(self, request: web.Request) -> web.Response:
        if not self._autorizado(request):
            return web.json_response({"erro": "não autorizado"}, status=401)
        return web.json_response({"reunioes": await asyncio.to_thread(listar)})

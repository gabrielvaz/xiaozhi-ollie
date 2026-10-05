"""Memória offline do Watcher: itens de texto + áudio que o aparelho baixa para o microSD.

Itens: resumo das sessões do Claude Code, últimas reuniões e lembretes/notas.
Áudio: voz do Ollie (mesmo TTS) convertida para Ogg Opus 16 kHz / 60 ms, o formato que o
firmware toca (AudioService::PlaySound). Os áudios ficam em cache pelo hash do texto.
"""

import hashlib
import json
import os
import subprocess
import time
from datetime import datetime
from pathlib import Path

import requests

from core.utils.icloud import ler_texto

ICLOUD = Path.home() / "Library/Mobile Documents/com~apple~CloudDocs/Watcher"
CACHE = Path(os.environ.get("WATCHER_CACHE_MEMORIA", Path(__file__).resolve().parents[2] / "data/memoria"))
FFMPEG = "/opt/homebrew/bin/ffmpeg"
_ultimo = {"quando": 0.0, "itens": []}


def _api(caminho: str, **kwargs):
    base = os.environ.get("API_BASE_URL", "https://openrouter.ai/api/v1")
    return requests.post(f"{base}{caminho}", headers={"Authorization": f"Bearer {os.environ.get('API_KEY', '')}"},
                         timeout=60, **kwargs)


def _resumo_sessoes(ponte) -> str:
    ativas = [a for a in ponte._agentes() if a["situacao"] != "parada"]
    if not ativas:
        return "Nenhuma sessão ativa ou concluída nas últimas horas."
    linhas = []
    for a in ativas[:8]:
        quando = a.get("minutos_desde_ultima_atividade")
        linhas.append(f"{a.get('titulo') or a.get('pasta')} — {a['situacao']}"
                      + (f", há {quando} min" if quando is not None else "")
                      + (f": {a.get('ultima_fala', '')[:300]}" if a.get("ultima_fala") else ""))
    try:
        r = _api("/chat/completions", json={
            "model": os.environ.get("MODELO_LLM", "openai/gpt-6-luna"), "max_tokens": 600,
            "reasoning": {"effort": "minimal"},
            "messages": [{"role": "system", "content": "Resuma para ser lido em voz alta, em português do Brasil, "
                          "em até 6 frases curtas, o estado destas sessões de agentes de código. Sem markdown."},
                         {"role": "user", "content": "\n".join(linhas)}]})
        r.raise_for_status()
        return r.json()["choices"][0]["message"]["content"].strip()
    except Exception:
        return " ".join(linhas)[:900]


def _reunioes(limite: int = 3) -> list[dict]:
    pasta = ICLOUD / "Reuniões"
    itens = []
    for d in sorted(pasta.glob("*/resumo.md"), reverse=True)[:limite]:
        texto = ler_texto(d, padrao="")
        titulo = texto.splitlines()[0].lstrip("# ").strip() if texto else d.parent.name
        corpo = " ".join(l.strip("-* ") for l in texto.splitlines()[1:] if l.strip() and not l.startswith("#"))
        itens.append({"titulo": titulo, "texto": corpo[:1200]})
    return itens


def _lembretes() -> str:
    arq = ICLOUD / "Lembretes.md"
    if not arq.exists():
        return ""
    itens = [l[2:].strip() for l in ler_texto(arq, padrao="").splitlines() if l.startswith("- ")]
    return "\n".join(itens[-15:])


def _audio(texto: str) -> str:
    """Gera (ou reaproveita) o .ogg do texto e devolve o nome do arquivo."""
    CACHE.mkdir(parents=True, exist_ok=True)
    nome = hashlib.sha1(texto.encode("utf-8")).hexdigest()[:16] + ".ogg"
    destino = CACHE / nome
    if destino.exists():
        return nome
    r = _api("/audio/speech", json={"model": os.environ.get("MODELO_TTS"), "voice": os.environ.get("VOZ"),
                                    "response_format": "mp3", "input": texto[:1500]})
    r.raise_for_status()
    mp3 = CACHE / (nome + ".mp3")
    mp3.write_bytes(r.content)
    subprocess.run([FFMPEG, "-y", "-loglevel", "error", "-i", str(mp3), "-ar", "16000", "-ac", "1",
                    "-c:a", "libopus", "-b:a", "24k", "-frame_duration", "60", "-application", "voip",
                    str(destino)], check=True)
    mp3.unlink(missing_ok=True)
    return nome


def gerar(ponte, idade_max_s: int = 600) -> list[dict]:
    """Lista de itens {id, titulo, detalhe, texto, audio}. Reaproveita o resultado por até 10 min."""
    if time.time() - _ultimo["quando"] < idade_max_s and _ultimo["itens"]:
        return _ultimo["itens"]
    agora = datetime.now().strftime("%d/%m %H:%M")
    brutos = [{"id": "sessoes", "titulo": "Sessões do Claude Code", "detalhe": f"Atualizado {agora}",
               "texto": _resumo_sessoes(ponte)}]
    for n, r in enumerate(_reunioes(), 1):
        brutos.append({"id": f"reuniao{n}", "titulo": r["titulo"], "detalhe": "Resumo da reunião", "texto": r["texto"]})
    lembretes = _lembretes()
    if lembretes:
        brutos.append({"id": "lembretes", "titulo": "Lembretes e notas", "detalhe": f"Atualizado {agora}",
                       "texto": lembretes})
    itens = []
    for item in brutos:
        try:
            item["audio"] = _audio(item["texto"])
        except Exception:
            item["audio"] = ""
        itens.append(item)
    _ultimo.update(quando=time.time(), itens=itens)
    (CACHE / "memoria.json").write_text(json.dumps(itens, ensure_ascii=False, indent=1), encoding="utf-8")
    return itens


def caminho_audio(nome: str) -> Path | None:
    arq = CACHE / Path(nome).name
    return arq if arq.suffix == ".ogg" and arq.exists() else None

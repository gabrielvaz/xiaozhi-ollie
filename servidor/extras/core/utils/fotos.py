"""Fotos tiradas pelo Watcher (pela voz ou pelo app Câmera), com a pergunta e a descrição da IA.

Pasta LOCAL: xiaozhi-server/data/fotos/AAAA-MM-DD HHhMMmSS.jpg + .json ({quando, pergunta, resposta}).
A cópia para iCloud Drive/Watcher/Fotos (ou WATCHER_PASTA_FOTOS) só acontece no app Backup.
Gravado pelo patch do vision_handler (patches/traduzir_servidor.py). Nunca derruba a análise da foto.
"""

import json
import os
from datetime import datetime
from pathlib import Path

PASTA = Path(__file__).resolve().parents[2] / "data/fotos"
ESPELHO = Path(os.environ.get("WATCHER_PASTA_FOTOS",
                              Path.home() / "Library/Mobile Documents/com~apple~CloudDocs/Watcher/Fotos"))


def guardar(imagem: bytes, pergunta: str, resposta) -> None:
    try:
        PASTA.mkdir(parents=True, exist_ok=True)
        agora = datetime.now()
        nome = agora.strftime("%Y-%m-%d %Hh%Mm%S")
        (PASTA / f"{nome}.jpg").write_bytes(imagem)
        texto = resposta if isinstance(resposta, str) else json.dumps(resposta, ensure_ascii=False)
        (PASTA / f"{nome}.json").write_text(json.dumps(
            {"quando": agora.isoformat(timespec="seconds"), "pergunta": pergunta, "resposta": texto},
            ensure_ascii=False), encoding="utf-8")
    except Exception:
        pass


def listar(limite: int = 30) -> list[dict]:
    from core.utils.vigia import rotulo_dia
    itens = []
    for meta in sorted(PASTA.glob("*.json"), reverse=True)[:limite]:
        try:
            d = json.loads(meta.read_text(encoding="utf-8"))
            quando = datetime.fromisoformat(d["quando"])
        except (OSError, ValueError, KeyError):
            continue
        resposta = " ".join(str(d.get("resposta", "")).split())
        itens.append({"id": meta.stem, "titulo": resposta[:60] or meta.stem,
                      "detalhe": f"{rotulo_dia(quando)} {quando:%H:%M}", "texto": resposta[:1500],
                      "pergunta": d.get("pergunta", "")[:200]})
    return itens


def espelhar() -> tuple[int, list[str]]:
    """Backup: copia as fotos para o iCloud (sem sobrescrever por arquivos menores)."""
    from core.utils.reuniao import _copiar_seguro
    copiados, falhas = 0, []
    for origem in PASTA.glob("*") if PASTA.is_dir() else []:
        destino = ESPELHO / origem.name
        if destino.exists() and destino.stat().st_size == origem.stat().st_size:
            continue
        if _copiar_seguro(origem, destino):
            copiados += 1
        else:
            falhas.append(origem.name)
    return copiados, falhas

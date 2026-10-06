"""Bloco <memoria> do prompt do Ollie: arquivos .md de iCloud Drive/Watcher/Memória (sobre-mim.md primeiro)
os lembretes guardados por voz e os resumos das conversas recentes (Watcher/Conversas). Lido a cada conversa, então editar o arquivo já vale na próxima."""

import threading
import time
from pathlib import Path

from core.utils.icloud import ler_texto

PASTA = Path.home() / "Library/Mobile Documents/com~apple~CloudDocs/Watcher"
LIMITE = 8000  # caracteres, para não pesar o prompt


_cache = {"texto": "", "quando": 0.0, "lendo": False}
_trava_cache = threading.Lock()
VALIDADE_S = 30


def ler() -> str:
    """Devolve a memória em cache na hora (ler o iCloud leva segundos e atrasava o início da conversa,
    descartando o começo da fala). Se o cache passou de 30 s, renova em segundo plano."""
    with _trava_cache:
        velho = time.time() - _cache["quando"] > VALIDADE_S
        if velho and not _cache["lendo"]:
            _cache["lendo"] = True
            threading.Thread(target=_renovar, daemon=True, name="memoria-usuario").start()
        return _cache["texto"]


def _renovar() -> None:
    try:
        texto = _ler()
    except Exception:
        texto = None  # a memória nunca pode derrubar a conversa
    with _trava_cache:
        if texto is not None:
            _cache["texto"] = texto
        _cache["quando"] = time.time()
        _cache["lendo"] = False


def _ler() -> str:
    partes = []
    pasta = PASTA / "Memória"
    arquivos = sorted(pasta.glob("*.md"), key=lambda p: (p.name != "sobre-mim.md", p.name)) if pasta.exists() else []
    for arq in arquivos:
        partes.append(ler_texto(arq, padrao="").strip())
    lembretes = PASTA / "Lembretes.md"
    if lembretes.exists():
        itens = [l for l in ler_texto(lembretes, padrao="").splitlines() if l.startswith("- ")]
        if itens:
            partes.append("## Lembretes guardados por voz\n" + "\n".join(itens[-20:]))
    from core.utils.diario import memoria_recente
    recentes = memoria_recente()
    if recentes:
        partes.append(recentes)
    texto = "\n\n".join(partes)
    return texto[:LIMITE].replace("{{", "{ {").replace("}}", "} }")


_renovar()  # já deixa a memória pronta quando o servidor sobe

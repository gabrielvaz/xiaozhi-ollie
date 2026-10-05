"""Bloco <memoria> do prompt do Ollie: arquivos .md de iCloud Drive/Watcher/Memória (sobre-mim.md primeiro)
os lembretes guardados por voz e os resumos das conversas recentes (Watcher/Conversas). Lido a cada conversa, então editar o arquivo já vale na próxima."""

from pathlib import Path

from core.utils.icloud import ler_texto

PASTA = Path.home() / "Library/Mobile Documents/com~apple~CloudDocs/Watcher"
LIMITE = 8000  # caracteres, para não pesar o prompt


def ler() -> str:
    try:
        return _ler()
    except Exception:
        return ""  # a memória nunca pode derrubar a conversa


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

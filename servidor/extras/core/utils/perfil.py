"""Nome do agente (escolhido nas Configurações do Watcher) e nome do usuário.

O Watcher informa o agente em GET /watcher/perfil?agente=Nome a cada inicialização; o nome fica em
data/perfil.json e troca "Ollie" no prompt, na ativação e no registro das conversas.
"""

import json
import os
from pathlib import Path

NOMES = ["Ollie", "Clawd", "Jarvis", "Nova", "Atlas", "Luna", "Max", "Iris"]
ARQUIVO = Path(__file__).resolve().parents[2] / "data/perfil.json"


def _ler() -> dict:
    try:
        return json.loads(ARQUIVO.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return {}


def nome_agente() -> str:
    nome = _ler().get("agente", "Ollie")
    return nome if nome in NOMES else "Ollie"


def definir_agente(nome: str) -> bool:
    nome = next((n for n in NOMES if n.lower() == (nome or "").strip().lower()), "")
    if not nome:
        return False
    if nome != nome_agente():
        dados = _ler()
        dados["agente"] = nome
        ARQUIVO.parent.mkdir(parents=True, exist_ok=True)
        ARQUIVO.write_text(json.dumps(dados, ensure_ascii=False), encoding="utf-8")
    return True


def usuario() -> str:
    return os.environ.get("NOME_USUARIO", "")


def aplicar(texto: str) -> str:
    """Troca o nome padrão (Ollie) pelo escolhido no prompt."""
    nome = nome_agente()
    return texto if nome == "Ollie" else texto.replace("Ollie", nome)

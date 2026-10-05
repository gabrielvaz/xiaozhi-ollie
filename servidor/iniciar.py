"""Inicializa o servidor do Watcher (usado pelo LaunchAgent).

Faz o mesmo que iniciar.sh, mas em Python: o macOS bloqueia o zsh iniciado pelo
launchd de ler o disco externo, e o Python do servidor tem acesso.
1. Lê o .env, gera xiaozhi-server/data/.config.yaml e .mcp_server_settings.json.
2. Substitui este processo pelo xiaozhi-server (mesmo Python).
"""

import json
import os
import re
import sys
from pathlib import Path

DIR = Path(__file__).resolve().parent
SRV = DIR / "xiaozhi-server"
ENV_FILE = Path(os.environ.get("ENV_FILE", DIR / ".env"))

if not ENV_FILE.exists():
    sys.exit(f"Falta o arquivo {ENV_FILE} (copie de .env.exemplo).")

env = {}
for linha in ENV_FILE.read_text(encoding="utf-8").splitlines():
    m = re.match(r"^([A-Z0-9_]+)=(.*)$", linha.strip())
    if m:
        env[m.group(1)] = m.group(2).strip().strip('"')
os.environ.update(env)
os.environ.update({
    "UV_CACHE_DIR": str(DIR / ".cache/uv"),
    "XDG_CACHE_HOME": str(DIR / ".cache"),
    "PATH": ":".join([str(Path.home() / ".local/bin"), "/opt/homebrew/bin", "/usr/local/bin", "/usr/bin", "/bin", "/usr/sbin", "/sbin"]),
})

# Idioma (IDIOMA no .env): nome para os prompts e voz padrão, se VOZ estiver vazia
sys.path.insert(0, str(DIR / "extras/core/utils"))
import idioma  # noqa: E402

os.environ["IDIOMA"] = idioma.IDIOMA
os.environ["IDIOMA_NOME"] = idioma.NOME
if not os.environ.get("VOZ"):
    os.environ["VOZ"] = idioma.VOZES[idioma.IDIOMA]

faltando = set()


def troca(m: re.Match) -> str:
    valor = os.environ.get(m.group(1))
    if not valor:
        faltando.add(m.group(1))
        return m.group(0)
    return valor


config = re.sub(r"\$\{([A-Z0-9_]+)\}", troca, (DIR / "config.template.yaml").read_text(encoding="utf-8"))
if faltando:
    sys.exit(f"Variáveis faltando no .env: {', '.join(sorted(faltando))}")

(SRV / "data").mkdir(exist_ok=True)
destino = SRV / "data/.config.yaml"
destino.write_text(config, encoding="utf-8")
destino.chmod(0o600)

mcp = {
    "mcpServers": {
        "mac-mini": {
            "command": str(SRV / ".venv/bin/python"),
            "args": [str(DIR / "ponte-mcp/ponte.py")],
            "env": {"PONTE_RAIZES": env.get("PONTE_RAIZES", str(Path.home() / "dev")), "HOME": str(Path.home())},
        }
    }
}
(SRV / "data/.mcp_server_settings.json").write_text(json.dumps(mcp, indent=2), encoding="utf-8")

# Ferramentas próprias (plugins próprios) -> plugins_func/functions
import shutil
for arq in (DIR / "plugins").glob("*.py"):
    shutil.copy(arq, SRV / "plugins_func/functions" / arq.name)
# Módulos próprios do servidor (extras/ espelha a árvore do xiaozhi-server)
for arq in (DIR / "extras").rglob("*.py"):
    destino_extra = SRV / arq.relative_to(DIR / "extras")
    destino_extra.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy(arq, destino_extra)

os.chdir(SRV)
python = str(SRV / ".venv/bin/python")
os.execv(python, [python, "app.py"])

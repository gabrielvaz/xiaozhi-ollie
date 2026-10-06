#!/bin/zsh
# Gera a configuração a partir do .env e sobe o xiaozhi-server com a ponte MCP.
set -euo pipefail

DIR="${0:A:h}"
cd "$DIR"

ENV_FILE="${ENV_FILE:-$DIR/.env}"
if [[ ! -f "$ENV_FILE" ]]; then
  echo "Falta o arquivo .env (copie de .env.exemplo)." >&2
  exit 1
fi

set -a; source "$ENV_FILE"; set +a
export UV_CACHE_DIR="$DIR/.cache/uv" XDG_CACHE_HOME="$DIR/.cache"
export PATH="$HOME/.local/bin:/opt/homebrew/bin:/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin"

SRV="$DIR/xiaozhi-server"
mkdir -p "$SRV/data"

"$SRV/.venv/bin/python" - "$DIR/config.template.yaml" "$SRV/data/.config.yaml" "$DIR" <<'PY'
import os, re, sys
sys.path.insert(0, os.path.join(sys.argv[3], "extras/core/utils"))
import idioma  # IDIOMA do .env: nome para os prompts e voz padrão
os.environ["IDIOMA"] = idioma.IDIOMA
os.environ["IDIOMA_NOME"] = idioma.NOME
os.environ["OLLIE_ERRO"] = idioma.t("Tive um problema agora. Tenta de novo daqui a pouco.",
                                    "Something went wrong. Please try again in a moment.",
                                    "刚才出了点问题，请稍后再试。",
                                    "He tenido un problema. Inténtalo de nuevo en un momento.")
if not os.environ.get("VOZ"):
    os.environ["VOZ"] = idioma.VOZES[idioma.IDIOMA]
texto = open(sys.argv[1], encoding="utf-8").read()
faltando = set()
def troca(m):
    v = os.environ.get(m.group(1))
    if not v:
        faltando.add(m.group(1))
        return m.group(0)
    return v
saida = re.sub(r"\$\{([A-Z0-9_]+)\}", troca, texto)
if faltando:
    sys.exit(f"Variáveis faltando no .env: {', '.join(sorted(faltando))}")
open(sys.argv[2], "w", encoding="utf-8").write(saida)
os.chmod(sys.argv[2], 0o600)
PY

cat > "$SRV/data/.mcp_server_settings.json" <<JSON
{
  "mcpServers": {
    "mac-mini": {
      "command": "$SRV/.venv/bin/python",
      "args": ["$DIR/ponte-mcp/ponte.py"],
      "env": {
        "PONTE_RAIZES": "${PONTE_RAIZES:-$HOME/dev}",
        "HOME": "$HOME"
      }
    }
  }
}
JSON

cd "$SRV"
exec .venv/bin/python app.py

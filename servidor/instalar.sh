#!/bin/zsh
# Monta o servidor do zero: baixa o xiaozhi-esp32-server no commit testado, aplica as
# traduções e correções, cria o ambiente Python e o .env com segredos novos.
# Depois: preencha API_KEY no .env, rode ./testar-api.sh e ./instalar-servico.sh instalar.
set -euo pipefail

DIR="${0:A:h}"
cd "$DIR"
COMMIT_SERVIDOR=87c6df77b0220ddbd8fc804f984f83f69f3225e5   # xinnan-tech/xiaozhi-esp32-server, 29/09/2026
export UV_CACHE_DIR="$DIR/.cache/uv" XDG_CACHE_HOME="$DIR/.cache"

if [[ ! -d xiaozhi-server ]]; then
  tmp=$(mktemp -d)
  git clone -q https://github.com/xinnan-tech/xiaozhi-esp32-server.git "$tmp/srv"
  git -C "$tmp/srv" checkout -q "$COMMIT_SERVIDOR"
  cp -R "$tmp/srv/main/xiaozhi-server" xiaozhi-server
  rm -rf "$tmp"
fi

cp arquivos/agent-base-prompt-ptbr.txt arquivos/requirements-mac.txt xiaozhi-server/

if [[ ! -x xiaozhi-server/.venv/bin/python ]]; then
  uv venv -q --python 3.10 xiaozhi-server/.venv
fi
uv pip install -q --python xiaozhi-server/.venv/bin/python -r xiaozhi-server/requirements-mac.txt

xiaozhi-server/.venv/bin/python patches/traduzir_servidor.py

if [[ ! -f .env ]]; then
  sed -e "s/^SEGREDO=.*/SEGREDO=$(openssl rand -hex 16)/" -e "s/^AUTH_KEY=.*/AUTH_KEY=$(openssl rand -hex 32)/" .env.exemplo > .env
  chmod 600 .env
  echo "Criado .env com SEGREDO e AUTH_KEY novos. Preencha API_KEY."
fi
echo "Servidor pronto em $DIR/xiaozhi-server"

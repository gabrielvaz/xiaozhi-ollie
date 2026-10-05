#!/bin/zsh
# Expõe o servidor na internet pelo Tailscale Funnel, só sob o caminho secreto do .env.
#   https://<HOST_PUBLICO>/<SEGREDO>/http/...  -> 127.0.0.1:8003 (OTA e visão)
#   wss://<HOST_PUBLICO>/<SEGREDO>/ws/...      -> 127.0.0.1:8000 (WebSocket de voz)
# Uso: ./configurar-funnel.sh [ligar|desligar|status]
set -euo pipefail
DIR="${0:A:h}"
set -a; source "$DIR/.env"; set +a

case "${1:-ligar}" in
  ligar)
    tailscale funnel --bg --set-path "/$SEGREDO/http" http://127.0.0.1:8003
    tailscale funnel --bg --set-path "/$SEGREDO/ws" http://127.0.0.1:8000
    echo
    echo "URL OTA para o portal de Wi-Fi do Watcher (campo Custom OTA URL):"
    echo "  https://$HOST_PUBLICO/$SEGREDO/http/xiaozhi/ota/"
    ;;
  desligar)
    tailscale funnel --set-path "/$SEGREDO/http" off || true
    tailscale funnel --set-path "/$SEGREDO/ws" off || true
    ;;
  status)
    tailscale funnel status | sed -E "s/$SEGREDO/<SEGREDO>/g"
    ;;
esac

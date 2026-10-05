#!/bin/zsh
# Instala o servidor como LaunchAgent: sobe no login e reinicia se cair.
# Uso: ./instalar-servico.sh [instalar|remover|reiniciar|log]
set -euo pipefail
DIR="${0:A:h}"
[[ -f "$DIR/.env" ]] && { set -a; source "$DIR/.env"; set +a; }
ROTULO="${WATCHER_ROTULO:-com.xiaozhi-ollie.servidor}"
PLIST="$HOME/Library/LaunchAgents/$ROTULO.plist"
LOG="$HOME/Library/Logs/watcher-servidor.log"
# Python real do venv (o launchd não deixa o zsh ler o disco externo; o Python consegue)
PYTHON="$(readlink -f "$DIR/xiaozhi-server/.venv/bin/python")"
mkdir -p "$DIR/logs"

case "${1:-instalar}" in
  instalar)
    cat > "$PLIST" <<XML
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>Label</key><string>$ROTULO</string>
  <key>ProgramArguments</key><array><string>$PYTHON</string><string>$DIR/iniciar.py</string></array>
  <key>WorkingDirectory</key><string>$HOME</string>
  <key>RunAtLoad</key><true/>
  <key>KeepAlive</key><true/>
  <key>ThrottleInterval</key><integer>15</integer>
  <key>StandardOutPath</key><string>$LOG</string>
  <key>StandardErrorPath</key><string>$LOG</string>
</dict>
</plist>
XML
    launchctl bootout "gui/$(id -u)/$ROTULO" 2>/dev/null || true
    launchctl bootstrap "gui/$(id -u)" "$PLIST"
    echo "Serviço instalado. Log: $LOG"
    ;;
  remover)
    launchctl bootout "gui/$(id -u)/$ROTULO" 2>/dev/null || true
    rm -f "$PLIST"
    ;;
  reiniciar)
    launchctl kickstart -k "gui/$(id -u)/$ROTULO"
    ;;
  log)
    tail -f "$LOG"
    ;;
esac

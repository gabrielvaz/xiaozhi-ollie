#!/bin/zsh
# Grava o XiaoZhi v2.5.0 no SenseCAP Watcher preservando a partição de fábrica da Seeed.
#
# 1. Acha a porta serial do ESP32-S3 (o Watcher mostra duas: ESP32 e Himax).
# 2. Faz backup da flash inteira (32 MB) e da nvsfactory em backups/.
# 3. Grava o merged-binary.bin em duas partes, pulando 0x9000..0x3B000 (nvsfactory).
#
# Usa o firmware Xiaozhi Ollie compilado (xiaozhi-ollie/saida) se existir; senão, o oficial.
# Uso: ./gravar-xiaozhi.sh [porta] [--oficial]     ex.: ./gravar-xiaozhi.sh /dev/cu.usbmodem1101
set -euo pipefail

DIR="${0:A:h}"
BIN="$DIR/02-xiaozhi-v2.5.0/merged-binary.bin"
if [[ " $* " != *" --oficial "* && -f "$DIR/xiaozhi-ollie/saida/merged-binary.bin" ]]; then
  BIN="$DIR/xiaozhi-ollie/saida/merged-binary.bin"
fi
set -- ${@:#--oficial}
echo "Firmware: $BIN"
BACKUPS="$DIR/backups"
export UV_CACHE_DIR="$DIR/../servidor/.cache/uv"
ESPTOOL=(uvx --from "esptool>=5,<6" esptool --chip esp32s3)

NVSFACTORY_INI=$((0x9000))
NVSFACTORY_FIM=$((0x9000 + 200 * 1024))   # 0x3B000

[[ -f "$BIN" ]] || unzip -o -q "$DIR/02-xiaozhi-v2.5.0/v2.5.0_sensecap-watcher.zip" -d "$DIR/02-xiaozhi-v2.5.0"

porta="${1:-}"
if [[ -z "$porta" ]]; then
  for p in /dev/cu.usbmodem* /dev/cu.usbserial* /dev/cu.wchusbserial*(N); do
    [[ -e "$p" ]] || continue
    if "${ESPTOOL[@]}" -p "$p" chip-id >/dev/null 2>&1; then porta="$p"; break; fi
  done
fi
[[ -n "$porta" ]] || { echo "Não achei o ESP32-S3. Conecte o USB-C de baixo do Watcher e passe a porta." >&2; exit 1; }
echo "ESP32-S3 em $porta"

mkdir -p "$BACKUPS"
carimbo=$(date +%Y%m%d-%H%M%S)
if ! ls "$BACKUPS"/watcher-flash-completa-*.bin >/dev/null 2>&1; then
  echo "Backup da flash inteira (32 MB, alguns minutos)..."
  "${ESPTOOL[@]}" -p "$porta" -b 2000000 read-flash 0x0 0x2000000 "$BACKUPS/watcher-flash-completa-$carimbo.bin"
fi
"${ESPTOOL[@]}" -p "$porta" -b 2000000 read-flash 0x9000 204800 "$BACKUPS/nvsfactory-$carimbo.bin"

parte1="$BACKUPS/.xiaozhi-parte1.bin"
parte2="$BACKUPS/.xiaozhi-parte2.bin"
head -c $NVSFACTORY_INI "$BIN" > "$parte1"
tail -c +$((NVSFACTORY_FIM + 1)) "$BIN" > "$parte2"

echo "Gravando XiaoZhi (sem tocar na nvsfactory)..."
"${ESPTOOL[@]}" -p "$porta" -b 2000000 write-flash --flash-mode dio --flash-size 32MB \
  0x0 "$parte1" $(printf '0x%X' $NVSFACTORY_FIM) "$parte2"

echo "Conferindo a nvsfactory..."
"${ESPTOOL[@]}" -p "$porta" -b 2000000 read-flash 0x9000 204800 "$BACKUPS/.nvsfactory-depois.bin"
if cmp -s "$BACKUPS/nvsfactory-$carimbo.bin" "$BACKUPS/.nvsfactory-depois.bin"; then
  echo "OK: nvsfactory intacta."
else
  echo "ATENÇÃO: nvsfactory mudou; restaurando o backup." >&2
  "${ESPTOOL[@]}" -p "$porta" -b 2000000 write-flash 0x9000 "$BACKUPS/nvsfactory-$carimbo.bin"
fi
rm -f "$parte1" "$parte2" "$BACKUPS/.nvsfactory-depois.bin"
echo "Pronto. O Watcher vai reiniciar e abrir o portal de Wi-Fi."

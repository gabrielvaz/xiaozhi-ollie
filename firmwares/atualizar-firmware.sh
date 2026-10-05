#!/bin/zsh
# Regrava só o programa e o pacote de imagens (mascote) do firmware compilado.
# Mantém as redes Wi-Fi e demais configurações. Para a primeira gravação use gravar-xiaozhi.sh.
# Uso: ./atualizar-firmware.sh [porta]
set -euo pipefail

DIR="${0:A:h}"
BUILD="$DIR/xiaozhi-ollie/xiaozhi-esp32/build"
export UV_CACHE_DIR="$DIR/../servidor/.cache/uv"
ESPTOOL=(uvx --from "esptool>=5,<6" esptool --chip esp32s3)

[[ -f "$BUILD/xiaozhi.bin" ]] || { echo "Compile antes: xiaozhi-ollie/compilar.sh" >&2; exit 1; }

porta="${1:-}"
if [[ -z "$porta" ]]; then
  for p in /dev/cu.usbmodem*(N); do
    if "${ESPTOOL[@]}" -p "$p" chip-id >/dev/null 2>&1; then porta="$p"; break; fi
  done
fi
[[ -n "$porta" ]] || { echo "Não achei o ESP32-S3. Conecte o USB-C de baixo do Watcher." >&2; exit 1; }

"${ESPTOOL[@]}" -p "$porta" -b 2000000 write-flash --flash-mode dio --flash-size 32MB \
  0x200000 "$BUILD/xiaozhi.bin" 0xa00000 "$BUILD/generated_assets.bin"
echo "Pronto: programa e mascote atualizados em $porta."

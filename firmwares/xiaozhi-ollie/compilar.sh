#!/bin/zsh
# Compila o XiaoZhi v2.5.0 para o SenseCAP Watcher: interface pt-BR, ativação "Hey Ollie",
# portal de Wi-Fi em pt-BR e endereço do servidor do Mac embutido.
# Saída: firmwares/xiaozhi-ollie/saida/merged-binary.bin (gravar com ../gravar-xiaozhi.sh --ptbr)
set -euo pipefail

AQUI="${0:A:h}"
# Caminhos da sua máquina (opcional, fora do git): firmwares/xiaozhi-ollie/local.env
[[ -f "$AQUI/local.env" ]] && source "$AQUI/local.env"
export IDF_PATH="${IDF_PATH:-$HOME/esp/esp-idf-v6.1}"
export IDF_TOOLS_PATH="${IDF_TOOLS_PATH:-$HOME/.espressif}"
export IDF_COMPONENT_CACHE_PATH="$IDF_TOOLS_PATH/component-cache"
export CCACHE_DIR="$IDF_TOOLS_PATH/ccache"
export XDG_CACHE_HOME="$IDF_TOOLS_PATH/cache"   # ~/.cache aponta para um volume que pode não estar montado

# Código de origem nas versões testadas (baixado só se faltar)
[[ -d "$AQUI/xiaozhi-esp32" ]] || git clone -q --depth 1 -b v2.5.0 https://github.com/78/xiaozhi-esp32.git "$AQUI/xiaozhi-esp32"
if [[ ! -d "$AQUI/esp-wifi-connect" ]]; then
  git clone -q https://github.com/78/esp-wifi-connect.git "$AQUI/esp-wifi-connect"
  git -C "$AQUI/esp-wifi-connect" checkout -q cc103899ea8199b0fbf18c9b6e65152d9a92091e   # v3.3.1
fi
[[ -f "$AQUI/fonte/NotoSans-Regular.ttf" ]] || curl -sSL -o "$AQUI/fonte/NotoSans-Regular.ttf" \
  https://raw.githubusercontent.com/notofonts/notofonts.github.io/main/fonts/NotoSans/hinted/ttf/NotoSans-Regular.ttf

python3 "$AQUI/aplicar_patches.py"

source "$IDF_PATH/export.sh" >/dev/null
cd "$AQUI/xiaozhi-esp32"
python scripts/build.py sensecap-watcher --language pt-BR --wake-word "custom:hey ollie|Ollie"

mkdir -p "$AQUI/saida"
cp build/merged-binary.bin "$AQUI/saida/merged-binary.bin"
grep -E '^CONFIG_(LANGUAGE_PT_BR|USE_CUSTOM_WAKE_WORD|CUSTOM_WAKE_WORD|SR_MN_EN_MULTINET7_QUANT|OTA_URL)' build/config/sdkconfig.h sdkconfig 2>/dev/null \
  | sed -E 's#/[0-9a-f]{32}/#/<SEGREDO>/#' | sort -u
ls -la "$AQUI/saida/merged-binary.bin"

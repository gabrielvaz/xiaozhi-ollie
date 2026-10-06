#!/bin/zsh
# Simula a tela do Watcher no Mac: compila, gera as cenas e confere se o Clawd está centrado.
# Resultado em saida/folha.png (fluxos) e saida/poses.png (todas as poses). Uso: ./rodar.sh [--abrir]
set -e
cd "$(dirname "$0")"
if [[ ! -d ../xiaozhi-esp32/managed_components/lvgl__lvgl ]]; then
  echo "Falta o código do XiaoZhi com os componentes: rode ../compilar.sh uma vez." >&2
  exit 1
fi
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release > /dev/null
cmake --build build -j8 2>&1 | grep -E "error" && exit 1 || true
rm -rf saida && mkdir -p saida/medida
OLLIE_MEDIR=1 ./build/simulador > /dev/null  # rodada de medição: Clawd sem acessórios
./build/simulador > /dev/null
resultado=0
python3 medir.py || resultado=$?
[[ "$1" == "--abrir" ]] && open saida/folha.png saida/poses.png
exit $resultado

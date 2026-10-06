#!/bin/zsh
# Mostra o diagnóstico do Watcher: reinícios anormais (com o rastro do que acontecia antes),
# buracos entre pulsos (congelou, ficou sem rede ou desligou) e a memória livre ao longo do tempo.
# Uso: ./ver-diagnostico.sh [horas]   (padrão: 24)
exec < /dev/null
ARQ="${0:A:h}/xiaozhi-server/data/diagnostico.log"
[[ -f "$ARQ" ]] || { echo "Ainda não há diagnóstico ($ARQ)."; exit 0; }
python3 - "$ARQ" "${1:-24}" <<'PY'
import json, sys
from datetime import datetime, timedelta
limite = datetime.now() - timedelta(hours=float(sys.argv[2]))
pulsos = 0
for linha in open(sys.argv[1], encoding="utf-8"):
    try:
        e = json.loads(linha)
    except ValueError:
        continue
    quando = datetime.fromisoformat(e["quando"])
    if quando < limite:
        continue
    if e["tipo"] == "pulso":
        pulsos += 1
        if pulsos % 30 == 1:  # uma amostra a cada ~30 min
            print(f"{e['quando']}  pulso  ligado {e['ligado_s'] // 60} min  heap {e['heap_k']}k (mín {e['heap_min_k']}k, "
                  f"maior bloco {e['maior_bloco_k']}k)  psram {e['psram_k']}k  estado {e['estado']}")
    elif e["tipo"] == "buraco":
        print(f"{e['quando']}  ** SEM PULSO por {e['sem_pulso_s'] // 60} min: {e['explicacao']}")
    elif e["tipo"] == "sessao_anterior":
        print(f"{e['quando']}  ligou de novo ({e.get('motivo_deste_inicio')}). Fim da sessão anterior (microSD):")
        for r in e.get("rastro", "").splitlines()[-15:]:
            print(f"      {r}")
    elif e["tipo"] == "reinicio":
        print(f"{e['quando']}  ** REINICIOU: {e['motivo']}. Últimos eventos antes disso:")
        for r in e.get("rastro", "").splitlines():
            print(f"      {r}")
print(f"({pulsos} pulsos no período)")
PY

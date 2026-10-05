#!/bin/zsh
# Mantém os dois repositórios em dia:
#   origin  = público (só o código, sem nada pessoal)
#   privado = pessoal (o mesmo código + a pasta pessoal/ com dados do dono; nunca segredos)
#
# 1. Confere todos os arquivos versionados contra .termos-pessoais (arquivo local, fora do git)
#    e os segredos do servidor/.env; se achar algo, para sem enviar nada.
# 2. Envia o main para o público.
# 3. Monta o repositório pessoal numa worktree (.privado/): código do main + pessoal/ e envia.
#
# Uso: ./sincronizar-repos.sh        (depois de fazer commit no main)
set -euo pipefail
exec < /dev/null

RAIZ="${0:A:h}"
cd "$RAIZ"
TERMOS="$RAIZ/.termos-pessoais"
ENV="$RAIZ/servidor/.env"

[[ "$(git rev-parse --abbrev-ref HEAD)" == "main" ]] || { echo "Rode no branch main." >&2; exit 1; }
[[ -z "$(git status --porcelain --untracked-files=no)" ]] || echo "Aviso: há mudanças sem commit; só o que está commitado é enviado."

# ---------------------------------------------------------------- 1. checagem
padroes=()
[[ -f "$TERMOS" ]] && padroes+=("${(@f)$(grep -vE '^\s*(#|$)' "$TERMOS")}")
if [[ -f "$ENV" ]]; then  # valores secretos do .env também não podem aparecer
  for chave in API_KEY SEGREDO AUTH_KEY; do
    valor=$(grep -E "^${chave}=" "$ENV" | head -1 | cut -d= -f2- | tr -d '"' || true)
    [[ ${#valor} -ge 8 ]] && padroes+=("$valor")
  done
fi
if (( ${#padroes} )); then
  achados=$(git grep -I -n -i -F "${(@)padroes/#/-e}" HEAD -- . 2>/dev/null || true)
  achados+=$(git log origin/main..HEAD --format='%an %ae %B' 2>/dev/null | grep -i -F "${(@)padroes/#/-e}" || true)
  if [[ -n "$achados" ]]; then
    echo "PARADO: dados pessoais no que iria para o público:" >&2
    echo "$achados" | cut -c1-160 >&2
    exit 1
  fi
fi
echo "Checagem ok: nada pessoal nos arquivos versionados."

# ---------------------------------------------------------------- 2. público
git push -q origin main
echo "Público atualizado: $(git log --oneline -1)"

# ---------------------------------------------------------------- 3. pessoal
WT="$RAIZ/.privado"
git fetch -q privado
if [[ ! -d "$WT" ]]; then
  git worktree add -q -B privado-main "$WT" privado/main
fi
cd "$WT"
git merge -q --ff-only privado/main 2>/dev/null || true
# código: espelha o main (menos pessoal/)
git ls-files | grep -v '^pessoal/' | while IFS= read -r f; do rm -f -- "$f"; done
git -C "$RAIZ" archive HEAD | tar -x -C "$WT"
# pessoal/: dados do dono que não podem ir ao público (sem chaves nem senhas)
mkdir -p pessoal
cp "$TERMOS" pessoal/termos-pessoais.txt 2>/dev/null || true
cp "$RAIZ/firmwares/xiaozhi-ollie/local.env" pessoal/firmware-local.env 2>/dev/null || true
MEMORIA="$HOME/Library/Mobile Documents/com~apple~CloudDocs/Watcher/Memória"
[[ -d "$MEMORIA" ]] && cp "$MEMORIA"/*.md pessoal/ 2>/dev/null || true
if [[ -f "$ENV" ]]; then  # só as variáveis que não são segredo
  grep -E '^(NOME_USUARIO|WATCHER_[A-Z_]+|IDIOMA|MODELO_[A-Z_]+|VOZ|TTS_FORMATO|HOST_PUBLICO|PONTE_RAIZES|API_BASE_URL)=' "$ENV" \
    > pessoal/env-pessoal.txt || true
fi
cat > pessoal/README.md <<'LEIA'
# pessoal/

Só existe no repositório pessoal. Gerada por `sincronizar-repos.sh` a cada sincronização:

- `env-pessoal.txt`: variáveis do `servidor/.env` que não são segredo (nome, vocabulário, modelos, host).
  Chaves e senhas (API_KEY, SEGREDO, AUTH_KEY) nunca entram aqui.
- `firmware-local.env`: caminhos do ESP-IDF desta máquina.
- `*.md`: memória do agente (iCloud Drive/Watcher/Memória).
- `termos-pessoais.txt`: lista usada para garantir que nada disso vá ao repositório público.
LEIA
git add -A
if git diff --cached --quiet; then
  echo "Pessoal já estava em dia."
else
  git commit -q -m "Sincroniza com o público ($(git -C "$RAIZ" log --oneline -1 | cut -c1-60))"
  git push -q privado privado-main:main
  echo "Pessoal atualizado: $(git log --oneline -1)"
fi

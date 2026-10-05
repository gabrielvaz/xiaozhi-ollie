#!/bin/zsh
# Faz uma chamada real de cada serviço do provedor configurado no .env:
#   1. modelo de conversa com uma ferramenta (o assistente depende disso)
#   2. voz (TTS) em pt-BR, salva em logs/teste-voz.<formato> e toca no Mac
#   3. transcrição (ASR) do áudio gerado no passo 2
# Uso: ./testar-api.sh [--sem-som]
DIR="${0:A:h}"
set -a; source "$DIR/.env"; set +a
mkdir -p "$DIR/logs"
ok()   { print -P "%F{green}✔%f $1"; }
erro() { print -P "%F{red}✘%f $1"; }
[[ -n "${API_KEY:-}" ]] || { erro "API_KEY vazia no .env"; exit 1; }
AUTH=(-H "Authorization: Bearer $API_KEY")
falhas=0

# 1. Conversa com ferramenta
resp=$(curl -s "$API_BASE_URL/chat/completions" "${AUTH[@]}" -H 'Content-Type: application/json' -d "{
  \"model\": \"$MODELO_LLM\",
  \"messages\": [{\"role\": \"user\", \"content\": \"Como estão minhas sessões do Claude?\"}],
  \"tools\": [{\"type\": \"function\", \"function\": {\"name\": \"sessoes_listar\", \"description\": \"Lista as sessões de agentes no herdr\", \"parameters\": {\"type\": \"object\", \"properties\": {}}}}]
}")
if echo "$resp" | grep -q '"sessoes_listar"'; then
  ok "Conversa: $MODELO_LLM chamou a ferramenta"
elif echo "$resp" | grep -q '"choices"'; then
  erro "Conversa: $MODELO_LLM respondeu, mas não chamou a ferramenta (troque o modelo)"; falhas=$((falhas+1))
else
  erro "Conversa: $(echo $resp | head -c 300)"; falhas=$((falhas+1))
fi

# 2. Voz
audio="$DIR/logs/teste-voz.$TTS_FORMATO"
code=$(curl -s -o "$audio" -w '%{http_code}' "$API_BASE_URL/audio/speech" "${AUTH[@]}" -H 'Content-Type: application/json' -d "{
  \"model\": \"$MODELO_TTS\", \"voice\": \"$VOZ\", \"response_format\": \"$TTS_FORMATO\", \"speed\": 1.05,
  \"input\": \"Oi! Você tem três sessões trabalhando e uma esperando aprovação.\"
}")
if [[ "$code" == 200 ]] && [[ $(stat -f%z "$audio") -gt 2000 ]]; then
  ok "Voz: $MODELO_TTS ($VOZ) gerou $(( $(stat -f%z "$audio") / 1024 )) KB"
  [[ "${1:-}" == "--sem-som" ]] || afplay "$audio"
else
  erro "Voz: HTTP $code $(head -c 300 "$audio")"; falhas=$((falhas+1)); audio=""
fi

# 3. Transcrição do áudio gerado
if [[ -n "$audio" ]]; then
  resp=$(curl -s "$API_BASE_URL/audio/transcriptions" "${AUTH[@]}" -F "model=$MODELO_ASR" -F "file=@$audio")
  texto=$(echo "$resp" | python3 -c 'import json,sys; print(json.load(sys.stdin).get("text",""))' 2>/dev/null)
  if [[ -n "$texto" ]]; then
    ok "Transcrição: $MODELO_ASR entendeu: \"$texto\""
  else
    erro "Transcrição: $(echo $resp | head -c 300)"; falhas=$((falhas+1))
  fi
fi

exit $falhas

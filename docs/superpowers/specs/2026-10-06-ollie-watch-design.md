# Ollie Watch: design

Data: 2026-10-06 · Status: aprovado pelo dono para implementação e envio ao TestFlight

## Objetivo

App de Apple Watch (SwiftUI, sem app de iPhone) que acompanha e comanda as sessões do Claude Code de qualquer pessoa:

- ao abrir, mostra as sessões ativas e o progresso de cada uma (trabalhando, esperando você, concluída; o que está fazendo agora; há quanto tempo);
- notifica quando uma sessão termina ou pede permissão, com Permitir/Negar na própria notificação;
- conversa por voz com a sessão: o ditado do watchOS vira prompt, a resposta é lida em voz alta pelo `AVSpeechSynthesizer`;
- mostra o uso do plano (janela de 5 horas e semanal) numa tela e numa complicação.

Distribuível: qualquer pessoa instala o plugin no Claude Code e pareia o relógio. Nenhum servidor roda no computador do usuário.

## Decisões e o porquê

| Decisão | Por quê |
|---|---|
| Sem "Entrar com Claude" | A Anthropic proíbe apps de terceiros de usar o login do Claude.ai ou guardar tokens da assinatura (code.claude.com/docs/en/legal-and-compliance). O vínculo é por pareamento com o plugin |
| Plugin do Claude Code (hooks + status line + channel) | É configuração, não servidor: o próprio Claude Code chama os hooks e inicia o channel |
| Relay hospedado por nós em Cloudflare Workers + Durable Objects | Alguém precisa mandar o push do APNs; um Durable Object por pareamento guarda estado, fila de prompts e conexões |
| herdr fora da v1 | herdr 0.9.3 só tem socket local e SSH; a Herdr Cloud ainda não existe. O plugin funciona dentro do herdr do mesmo jeito |
| Voz com o ditado e a síntese do próprio relógio | Sem chave de API, sem custo por uso, sem WebSocket no relógio (TN3135) |
| Tudo o que é ditado vira prompt para a sessão escolhida | O Claude Code é o cérebro; estado e uso são telas, não comandos de voz |
| Uso do plano pelo `rate_limits` da status line | É a única fonte oficial. Não usamos `/api/oauth/usage` nem o token do Keychain |
| Nome sem "Claude" | Regra de marca da Anthropic |

## Peças

```
Claude Code ── plugin ollie-watch ── hooks / status line ──HTTPS──▶ Relay (Worker + DO por pareamento) ──APNs──▶ Apple Watch
                                 └── channel (MCP stdio)  ◀──WSS───┘                                    ◀─HTTPS──┘
```

### 1. Relay (`apple-watch/relay/`, TypeScript, Cloudflare Workers)

- `PairingDO`: um por pareamento. Guarda sessões, uso, permissões pendentes, tokens de push do relógio e os WebSockets dos channels (com hibernação).
- `CodesDO`: um só, mapeia código curto → pareamento por 10 minutos.
- Tokens: `<pairingId>.<segredo>`; o DO guarda só o SHA-256 do segredo. Um token para o relógio, um para cada computador pareado.
- APNs: JWT ES256 assinado com WebCrypto, renovado a cada 50 min. Segredos `APNS_KEY_P8`, `APNS_KEY_ID`, `APNS_TEAM_ID`, `APNS_TOPIC`.

Rotas (todas JSON):

| Quem | Método e caminho | O que faz |
|---|---|---|
| relógio | `POST /v1/pair/start` | cria pareamento e código; devolve `{code, watchToken, expiresAt}` |
| relógio | `GET /v1/pair/status` | `{paired, hosts}` |
| plugin | `POST /v1/pair/claim` `{code, host}` | devolve `{token}` para o computador |
| relógio | `POST /v1/watch/push-token` `{token, sandbox}` | registra o token do APNs |
| relógio | `GET /v1/state` | sessões, uso, permissões pendentes |
| relógio | `POST /v1/sessions/:id/prompt` `{text}` | entrega o texto ao channel da sessão |
| relógio | `POST /v1/permissions/:id` `{decision: allow\|deny}` | responde a uma permissão |
| relógio | `DELETE /v1/pair` | desfaz o pareamento |
| plugin | `POST /v1/events` | evento de hook (`SessionStart`, `UserPromptSubmit`, `PreToolUse`, `Stop`, `Notification`, `SessionEnd`) |
| plugin | `POST /v1/usage` | `rate_limits` da status line |
| plugin | `POST /v1/permissions` e `GET /v1/permissions/:id/wait` | abre uma permissão e espera a resposta (long-poll de 25 s) |
| plugin | `GET /v1/channel?session=…` (WebSocket) | channel da sessão recebe prompts e pedidos de permissão |

Push:
- `Stop`: "Concluída: <título>" + a última fala (até 180 caracteres). Se o prompt veio do relógio, o push leva a fala inteira em `reply` para ser lida.
- Permissão: categoria `PERMISSION` com ações Permitir e Negar.
- Esperando você (`Notification`): "<título> está esperando você".

### 2. Plugin (`.claude-plugin/marketplace.json` na raiz, plugin em `apple-watch/plugin/`)

Node ≥ 22, sem dependências de npm.

- `hooks/hooks.json` chama `scripts/hook.mjs <evento>`, que lê o JSON do stdin e manda ao relay. Falhas de rede nunca travam o Claude Code: timeout curto e saída silenciosa.
- `PermissionRequest`: abre a permissão no relay e espera até `OLLIE_WATCH_PERMISSION_WAIT` segundos (padrão 90). Sem resposta, sai sem decidir e o terminal pergunta normalmente.
- Status line: `scripts/statusline.mjs` repassa `rate_limits` (no máximo 1 vez por minuto) e imprime a status line anterior do usuário, se houver.
- Channel `ollie-watch`: servidor MCP por stdio que abre um WebSocket de saída para o relay e injeta os prompts do relógio na sessão.
- Comando `/ollie-watch:pair <código>`: troca o código pelo token, salva em `~/.config/ollie-watch/config.json` (modo 600) e oferece configurar a status line.
- Comando `/ollie-watch:status`: mostra se está pareado e se o channel está ativo.

### 3. App (`apple-watch/app/`, SwiftUI, watchOS 11+)

Telas:
1. **Parear**: código grande, instrução de uma linha (`/plugin install …` e `/ollie-watch:pair K7Q-42M`), consulta o pareamento a cada 2 s.
2. **Sessões** (tela inicial): barras de uso 5 h e semana no topo; permissões pendentes em destaque; uma linha por sessão com estado (cor + ícone), título, o que está fazendo e tempo decorrido. Atualiza a cada 3 s com o app aberto.
3. **Sessão**: estado, atividade, última fala (rolável), botões **Falar** (ditado → prompt), **Ouvir** (lê a última fala) e cartão de permissão com Permitir/Negar. Depois de um prompt pelo relógio, lê a resposta em voz alta quando a sessão termina.
4. **Uso**: dois medidores com o horário de reinício.
5. **Ajustes**: ler respostas em voz alta (liga/desliga), desfazer pareamento, versão.

Complicações (WidgetKit): circular com o uso de 5 h; retangular com "2 trabalhando · 1 esperando". Dados compartilhados por App Group, recarregados quando o app atualiza o estado.

Idiomas: português (Brasil) e inglês.

## Erros e limites

- Relay fora do ar: hooks saem em silêncio; o app mostra "sem conexão" e o último estado salvo.
- Channel ausente (sem `--dangerously-load-development-channels`): o relay responde 409 ao prompt; o app explica como ligar.
- Sessão sem evento há 6 horas some da lista; `SessionEnd` remove na hora.
- Permissão sem resposta expira em 10 minutos no relay.
- Uso só aparece em planos Pro e Max, depois da primeira resposta de uma sessão.

## Testes

- Relay: testes com `vitest` + `@cloudflare/vitest-pool-workers` cobrindo pareamento, autenticação, eventos → estado, permissão (long-poll), prompt → channel.
- Plugin: teste de ponta a ponta com `node --test` contra o relay rodando em `wrangler dev`.
- App: testes unitários do decodificador de estado e da formatação; build para o simulador do watchOS.

## Entrega

- Relay publicado em `*.workers.dev` com os segredos do APNs.
- App enviado ao TestFlight (teste interno) com bundle `com.example.olliewatch`.
- Plugin publicado no repositório público só depois da confirmação do dono.

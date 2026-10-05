# Servidor do Ollie

O SenseCAP Watcher roda o firmware Xiaozhi Ollie e fala com este servidor no seu Mac, em vez da nuvem do xiaozhi.me.
O servidor usa qualquer API compatível com a da OpenAI (o OpenRouter, por exemplo) para entender a voz, pensar e falar; os modelos ficam no `.env`. As ações passam pela **ponte MCP**, que controla o herdr, o Claude Code, o Codex, o Multica e o Mac.

```
Watcher (Xiaozhi Ollie)
  │  wss / https, de qualquer lugar (Wi-Fi de casa ou hotspot do celular)
  ▼
Tailscale Funnel  https://<seu-mac>.<sua-tailnet>.ts.net/<SEGREDO>/...
  │  só o caminho secreto é publicado; a raiz não existe
  ▼
xiaozhi-server (127.0.0.1:8000 e :8003)  ── API compatível com OpenAI: transcrição, modelo, voz
  │  stdio
  ▼
ponte-mcp/ponte.py ── herdr · claude -p · codex exec · multica · Atalhos do macOS
```

## O que dá para pedir

| Área | Exemplos | Ferramentas |
|---|---|---|
| Sessões no herdr | "Como estão minhas sessões?", "O que a sessão do checkout fez?", "Alguma esperando aprovação?" | `sessoes_listar`, `sessao_ler` |
| Agir numa sessão | "Manda a sessão da api rodar os testes", "Aprova o pedido da sessão do site" | `sessao_instruir`, `sessao_responder` |
| Perguntar sem mexer | "Pergunta ao Claude como está o PR aberto", "Pergunta ao Codex o que falta na api" | `claude_perguntar`, `codex_perguntar`, `tarefa_resultado` |
| Trabalho novo | "Abre um Claude no repositório docs para revisar o README" | `claude_nova_sessao`, `codex_nova_sessao` |
| Multica | "Quais issues estão abertas?", "Cria uma issue para o agente de backend", "Dispara o resumo diário" | `multica_*` |
| Mac | "Como está o Mac?", "Roda o atalho Abrir App", "Mostra um aviso no Mac" | `mac_status`, `mac_atalho_*`, `mac_notificar` |

### Segurança

- **Confirmação por voz:** tudo que muda algo (instruir sessão, aprovar, abrir sessão, criar issue, rodar atalho) primeiro devolve `PRECISA_CONFIRMAR`. O Watcher repete o pedido e só executa depois do seu "sim".
- **Perguntas só leem:** `claude_perguntar` não pode editar nem rodar comandos além de `git status/log/diff` e `gh pr view/list/checks`. `codex_perguntar` roda com `--sandbox read-only`.
- **Sem shell livre:** ações no Mac só por Atalhos do app Shortcuts. Para uma ação nova, crie um atalho.
- **Teclas limitadas:** para destravar uma sessão, só enter, esc, tab, setas, y/n, 1 a 4 e ctrl+c.
- **Acesso:** o servidor só escuta em 127.0.0.1. A internet chega pelo Funnel, num caminho secreto de 32 caracteres hex, e o WebSocket exige um token assinado com `AUTH_KEY`.
- **O que sai do Mac:** só áudio, texto e resultados das ferramentas, enviados ao OpenRouter (e dele aos fornecedores dos modelos). Nada passa pelo xiaozhi.me.

## Arquivos

| Arquivo | Para quê |
|---|---|
| `.env` | Chave da API, modelos, voz, host público, seu nome, `SEGREDO` e `AUTH_KEY` (gerados). Fica só no Mac, chmod 600, fora do git |
| `config.template.yaml` | Configuração do xiaozhi-server: português, OpenAI, prompt e autenticação |
| `iniciar.sh` | Gera `xiaozhi-server/data/.config.yaml` e o arquivo de MCP, e sobe o servidor |
| `ponte-mcp/ponte.py` | A ponte MCP com as ferramentas (sessões, Claude, Codex, Multica, Mac, tempo, lembretes, uso do Claude) |
| `configurar-funnel.sh` | `ligar` / `desligar` / `status` do Funnel |
| `instalar-servico.sh` | `instalar` / `remover` / `reiniciar` / `log` do LaunchAgent (sobe no login) |
| `patches/traduzir_servidor.py` | Traduz para pt-BR as frases do servidor que chegam à tela ou à voz (rode de novo se atualizar o xiaozhi-server) |
| `xiaozhi-server/agent-base-prompt-ptbr.txt` | Prompt-base em português (o original era inglês com exemplos em chinês) |
| `testar-api.sh` | Chamada real de conversa (com ferramenta), voz e transcrição no provedor do `.env` |
| `verificar.sh` | Checa a chave da API, os modelos, o herdr, o Claude, o Codex, o Multica, o servidor e o Funnel |
| `../firmwares/xiaozhi-ollie/` | Firmware compilado: `compilar.sh` + `aplicar_patches.py` (interface pt-BR, "Hey Ollie", portal pt-BR, servidor embutido, mascote Clawd) |
| `../firmwares/xiaozhi-ollie/mascote-clawd/` | `gerar.py` desenha todos os GIFs do Clawd (emoções, estados e poses); `previa.png` mostra todos |
| `../firmwares/gravar-xiaozhi.sh` | Faz backup e grava o XiaoZhi sem apagar a partição de fábrica da Seeed |

## Instalação

O passo a passo completo, com um prompt pronto para fazer a configuração com o Claude Code ou o Codex, está em [`../docs/INSTALL.md`](../docs/INSTALL.md).

## Limitações conhecidas

- **Palavra de ativação:** "Hey Ollie" (MultiNet7 em inglês, com fonemas gerados no aparelho; não existe modelo em português). Clicar na roda também começa a conversa.
- **Perguntas longas:** o Claude ou o Codex têm até 45 s. Depois disso a resposta vira tarefa (`tarefa_resultado`) e o Watcher avisa que você pode perguntar mais tarde.
- **Bateria:** 400 mAh, é só reserva. Fora de casa, use um power bank USB-C.
- **Dependências:** o `vosk` foi removido do `requirements` porque não tem build para macOS (`requirements-mac.txt`). O cache do uv fica em `servidor/.cache`, para não depender do `~/.cache` do sistema.

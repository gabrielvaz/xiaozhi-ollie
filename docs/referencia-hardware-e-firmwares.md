# SenseCAP Watcher: hardware, software e firmwares pesquisados

Pasta de referência do meu SenseCAP Watcher: manual, especificações, firmwares prontos e código-fonte dos firmwares alternativos.
Levantamento feito em 05/10/2026.

## Estrutura

```
sensecap-watcher/
├── README.md                 ← este arquivo
├── manual/                   ← documentação oficial da Seeed (Markdown, do repo OSHW)
├── hardware/                 ← esquemático e datasheets (PDF)
├── firmwares/                ← binários prontos para gravar
│   ├── 01-oficial-factory-v1.1.7/   firmware de fábrica (SenseCraft)
│   ├── 02-xiaozhi-v2.5.0/           XiaoZhi AI, build oficial para o Watcher
│   └── 03-himax/                    firmware do chip de IA Himax + guia de gravação
├── repos/                    ← código-fonte (git clone --depth 1) dos firmwares alternativos
└── servidor/                 ← servidor próprio no Mac mini + ponte MCP (herdr, Claude, Codex, Multica, Mac)
```

**Configuração em uso:** XiaoZhi v2.5.0 apontado para o servidor próprio no Mac mini (sem xiaozhi.me), acessível de fora de casa pelo Tailscale Funnel. O roteiro de instalação está em [`servidor/README.md`](servidor/README.md).

## Especificações de hardware

| Item | Especificação |
|---|---|
| MCU | ESP32-S3 a 240 MHz, 8 MB PSRAM, 32 MB flash |
| Processador de IA | Himax HX6538 / WiseEye2 (Cortex-M55 + Ethos-U55), 16 MB flash |
| Câmera | OV5647, FOV 120°, foco fixo em 3 m |
| Tela | Touchscreen redonda de 1,45", 412×412 |
| Áudio | 1 microfone, alto-falante de 1 W (codec ES8311) |
| Controles | Roda (gira para cima/baixo e aperta), botão RST no furo inferior, 1 LED RGB |
| Wi-Fi / BLE | 802.11 b/g/n 2,4 GHz (até 100 m), Bluetooth 5 LE, antena interna |
| Armazenamento | Slot microSD até 32 GB (FAT32) |
| Expansão | 1 Grove I2C, header fêmea 2×4 |
| USB-C | 1 atrás (só energia), 1 embaixo (energia e programação) |
| Energia | 5 V DC (recomendado 5 V/3 A), bateria Li-ion 3,7 V 400 mAh de backup |
| Temperatura de operação | 0 a 45 °C |
| Montagem | Parede, mesa ou suporte |

Arquivos em `hardware/`: `SenseCAP_Watcher_v1.0_SCH.pdf` (esquemático), `HX6538_datasheet.pdf`, `esp32-s3_datasheet.pdf`.
O modelo 3D do gabinete (`.stp`, 50 MB) está no repo [OSHW-SenseCAP-Watcher](https://github.com/Seeed-Studio/OSHW-SenseCAP-Watcher/tree/main/Hardware) e não foi baixado.

## Software

- **Dois chips, dois firmwares:** ESP32-S3 (Wi-Fi, tela, áudio, app) e Himax (modelos de visão). Ao ligar no USB aparecem **duas portas seriais**: uma do ESP32, outra do Himax.
- **Firmware de fábrica:** SenseCraft (app SenseCraft no celular, tarefas por voz ou texto, detecção de pessoa, pet e gesto, LLM na nuvem da Seeed ou local).
- **SDK:** ESP-IDF (v5.2.x no firmware oficial; o Muse usa v6.0.1). Exemplos em `repos/Seeed-Studio_SenseCAP-Watcher-Firmware/examples`.
- **Modelos de IA:** SenseCraft AI, gravados no Himax com `python-sscma`.
- **Integrações:** Home Assistant, Node-RED, MQTT, HTTP (ver `manual/`).

### Manual (`manual/`)

| Arquivo | Conteúdo |
|---|---|
| `sensecap_watcher_getting_started.md` | Primeiros passos |
| `watcher_operation_guideline.md` | Operação: botões, roda, menus |
| `watcher_hardware_overview.md` | Visão geral do hardware e pinagem |
| `sensecap_watcher_tasks.md` | Como criar tarefas |
| `sensecap_watcher_software_service_framework.md` | Arquitetura de serviços (nuvem / local) |
| `sensecap_watcher_local_deploy.md` | Rodar o LLM localmente |
| `watcher_architecture.md` | Arquitetura do firmware ESP32 |
| `integrate_watcher_to_ha.md` | Integração com Home Assistant |

## ⚠️ Antes de gravar QUALQUER firmware

Faça backup da partição de fábrica: ela guarda o EUI e as credenciais do servidor SenseCraft. Sem ela, não dá para voltar ao firmware original funcionando.

```sh
pip3 install --upgrade esptool
esptool.py --chip esp32s3 --baud 2000000 --before default_reset --after hard_reset --no-stub \
  read_flash 0x9000 204800 nvsfactory.bin
# Melhor ainda: backup da flash inteira (32 MB)
esptool.py --chip esp32s3 --baud 2000000 read_flash 0x0 0x2000000 watcher_full_backup.bin
```

Anote também as informações que aparecem na tela ao ligar (EUI, código etc.). Se a gravação não progredir, troque para a outra porta serial.

## Firmwares baixados (prontos para gravar)

| Pasta | Firmware | Como gravar |
|---|---|---|
| `01-oficial-factory-v1.1.7` | Fábrica SenseCraft v1.1.7 (última release oficial) | `esptool.py --chip esp32s3 -b 2000000 write_flash --flash_mode dio --flash_size 32MB --flash_freq 80m 0x0 bootloader/bootloader.bin 0x8000 partition_table/partition-table.bin 0x10d000 ota_data_initial.bin 0x110000 factory_firmware.bin 0x1910000 srmodels/srmodels.bin 0x1a10000 storage.bin` (descompacte o zip; o `ota_data_initial.bin`, que não vem no zip, está baixado ao lado) |
| `02-xiaozhi-v2.5.0` | XiaoZhi AI v2.5.0, build `sensecap-watcher` (o mais popular da comunidade; é o que vem na versão "Watcher for XiaoZhi") | `esptool.py --chip esp32s3 -b 2000000 write_flash 0x0 merged-binary.bin`. Depois, ativar com o código de 6 dígitos em [xiaozhi.me](https://xiaozhi.me) |
| `03-himax` | Firmware Himax 2024-08-16 (só para recuperação; a Seeed não recomenda mexer) | Ver `03-himax/README-flash.md` (`python-sscma`, porta do Himax) |

## Firmwares por objetivo

Legenda: ✅ pronto para o Watcher · 🔧 suporta o Watcher, mas precisa compilar (ESP-IDF) · 🧩 existe para outra placa ESP32, precisa portar

### Claude Code

| Firmware | Status | O que faz | Pasta em `repos/` |
|---|---|---|---|
| [watcher-claude-usage](https://github.com/jeffrymahbuubi/watcher-claude-usage) | 🔧 | Mostrador de uso do Claude Code (janelas de 5 h e 7 dias), status do serviço e alertas falados. Roda sozinho no Watcher, sem Mac. **Não mostra as sessões**, só o consumo | `jeffrymahbuubi_watcher-claude-usage` |
| [claude-desktop-buddy](https://github.com/anthropics/claude-desktop-buddy) (oficial Anthropic) | 🧩 | API BLE oficial: mostra as sessões ativas do Claude Code / Cowork no app Desktop e permite **aprovar ou negar permissões** pelo aparelho. Referência feita para M5StickC Plus | `anthropics_claude-desktop-buddy` |
| [claude-desktop-buddy-esp32](https://github.com/MoveCall/claude-desktop-buddy-esp32) | 🧩 | Port em ESP-IDF da mesma ideia, com arquitetura de placas do XiaoZhi. **É o caminho mais curto para o Watcher**: o XiaoZhi já tem a placa `sensecap-watcher` pronta, então o port é copiar a definição da placa | `MoveCall_claude-desktop-buddy-esp32` |

Não achei firmware pronto que acompanhe sessões **remotas** do Claude Code no Watcher. O Buddy funciona por Bluetooth com o Claude Desktop rodando no Mac mini; com o Watcher perto do Mac, dá para acompanhar e aprovar. Para sessões fora de casa, o caminho continua sendo o Remote Control do Claude Code no celular.

### ChatGPT / OpenAI

| Firmware | Status | O que faz | Pasta em `repos/` |
|---|---|---|---|
| [openai-realtime](https://github.com/Seeed-Studio/SenseCAP-Watcher-Firmware/tree/main/examples/openai-realtime) (exemplo oficial Seeed) | 🔧 | Conversa por voz em tempo real com a OpenAI Realtime API (precisa de chave de API) | `Seeed-Studio_SenseCAP-Watcher-Firmware/examples/openai-realtime` |
| [sensecap-watcher-realtime](https://github.com/jmbowes/sensecap-watcher-realtime) | 🔧 | OpenAI Realtime com visão (câmera) e voz, timers, lembretes e notas, interface LVGL para a tela redonda | `jmbowes_sensecap-watcher-realtime` |
| XiaoZhi + [xiaozhi-esp32-server](https://github.com/xinnan-tech/xiaozhi-esp32-server) | ✅ firmware / servidor no Mac | Usa o firmware XiaoZhi já baixado, apontado para um servidor próprio configurado com modelos da OpenAI | `firmwares/02-xiaozhi-v2.5.0` |
| [wheatley-ai](https://github.com/pham-tuan-binh/wheatley-ai) | 🔧 | Voz em tempo real via LiveKit/WebRTC; o agente LiveKit pode usar a OpenAI | `pham-tuan-binh_wheatley-ai` |

### ChatGPT Dots

| Firmware | Status | O que faz | Pasta em `repos/` |
|---|---|---|---|
| [dots-device](https://github.com/usedhonda/dots-device) | 🧩 | Companheiro físico para o seu Dot: personagem animado, resumos curtos da conversa e perguntas de múltipla escolha respondidas por toque. Usa Secure MCP Tunnel + MCP Events. Feito para Waveshare ESP32-C6-Touch-LCD-1.47 (320×172, sem microfone); para o Watcher é preciso trocar driver de tela, touch (aqui seria a roda), pinos e layout de memória | `usedhonda_dots-device` |

O Dots foi lançado em 29/09/2026 e só existe esse projeto até agora (1 estrela, experimental). Requer ChatGPT Pro ou Business Premium. O próprio Dot pode receber acesso ao Mac pelo app ChatGPT desktop (desligado por padrão).

### Meta Muse

| Firmware | Status | O que faz | Pasta em `repos/` |
|---|---|---|---|
| [muse-gadget-sdk](https://github.com/facebookincubator/muse-gadget-sdk) (**oficial Meta**) | 🔧 | **Suporte oficial ao SenseCAP Watcher**: interface com personagem, push-to-talk, configurações e exibição de imagens enviadas pelo Muse. Compilar com `tools/muse/board.sh build watcher` e gravar com `tools/muse/board.sh flash watcher` (ESP-IDF v6.0.1). Precisa de token SDK em [gadgets.muse.ai](https://gadgets.muse.ai/settings/sdk-tokens) e do app Muse (Developer mode) para parear | `facebookincubator_muse-gadget-sdk` |
| [muse-gadget-xiaozhi](https://github.com/moerdowo/muse-gadget-xiaozhi) | 🧩 | Interface do Muse falando com o backend XiaoZhi em vez do Muse. Curiosidade; testado só na Waveshare AMOLED 1.8 | `moerdowo_muse-gadget-xiaozhi` |

O SDK do Muse também tem um **Linux Device SDK** (`linux/`) que transforma um computador em "gadget" com comandos próprios. Ele é o caminho para o Muse executar tarefas no servidor.

### Acesso remoto ao Mac mini

Nenhum firmware faz área de trabalho remota (VNC) no Watcher: a tela de 1,45" não comporta isso. O que funciona é **controlar o Mac por voz** a partir do Watcher:

| Caminho | Como funciona | Pasta em `repos/` |
|---|---|---|
| XiaoZhi + MCP no Mac | Roda no Mac um servidor MCP ligado ao endpoint MCP do XiaoZhi (`mcp_pipe.py`). Você expõe ferramentas (shell, AppleScript, abrir apps, status) e aciona pela voz no Watcher | `78_mcp-calculator` (exemplo oficial do endpoint MCP do XiaoZhi) |
| [xiaozhi-openclaw](https://github.com/Lara-srl/xiaozhi-openclaw) | Fork do XiaoZhi para o Watcher, integrado ao gateway OpenClaw (agente que roda no seu computador) | `Lara-srl_xiaozhi-openclaw` |
| Muse + Linux Device SDK | Muse pelo Watcher, com comandos executados no computador pelo SDK Linux | `facebookincubator_muse-gadget-sdk/linux` |
| ChatGPT Dots com acesso local | O Dot opera o Mac pelo app ChatGPT desktop; o Watcher vira o painel (via dots-device portado) | `usedhonda_dots-device` |

### Multica

[Multica](https://github.com/multica-ai/multica) (v0.6.1, 01/10/2026) é uma plataforma open source para gerenciar agentes de código (Claude Code, Codex, Cursor etc.) como colegas de equipe, com issues, runs e autopilots. **Não existe firmware para o Watcher nem para qualquer ESP32 que fale com o Multica** (busca no GitHub e na web em 05/10/2026). O Multica expõe o que é preciso para integrar:

- **CLI scriptável** com saída JSON: `multica issue list|create|comment add|runs`, `multica agent list`, `multica daemon status`, `multica runtime activity` (`--output json`).
- **Webhooks de autopilot**: um POST externo dispara um agente ou cria uma issue.
- **WebSocket de tempo real** (issues, chat, inbox, presença), usado pelos apps web e mobile.
- **Tokens de acesso pessoais** (`multica login --token`).

Caminhos para o Watcher, do mais simples ao mais trabalhoso:

| Caminho | Firmware | O que dá para fazer |
|---|---|---|
| **XiaoZhi + servidor MCP no Mac embrulhando a CLI `multica`** (recomendado) | ✅ `firmwares/02-xiaozhi-v2.5.0` (sem compilar) | Por voz: "o que os agentes estão fazendo?", "cria uma issue para o agente de backend", "comenta na MUL-123", "cancela a run". O servidor MCP roda no Mac mini e se liga ao endpoint MCP do XiaoZhi (base: `repos/78_mcp-calculator`) |
| **Botão/voz → webhook de autopilot** | ✅ XiaoZhi (ferramenta MCP que faz o POST) | Disparar autopilots prontos ("roda o resumo diário", "checa dependências") |
| **Painel dedicado na tela redonda** | 🔧 adaptar `repos/jeffrymahbuubi_watcher-claude-usage` | Mostrador fixo de runs ativas, bloqueios e issues aguardando revisão, consultando a API do Multica |

### Outros firmwares da comunidade (clonados)

| Repo | O que é |
|---|---|
| `pham-tuan-binh_watcher-mochi` | Pet de mesa estilo Dasai Mochi |
| `isaacarbone_WatcherOS` | Firmware multi-app: radar de voos, timer, provisionamento Wi-Fi |

## Fontes

- Seeed Wiki: [especificações de hardware](https://wiki.seeedstudio.com/watcher_hardware_overview/), [primeiros passos](https://wiki.seeedstudio.com/getting_started_with_watcher/), [gravar modelos de IA](https://wiki.seeedstudio.com/visual_trigger_and_ai_flash/)
- GitHub: [OSHW-SenseCAP-Watcher](https://github.com/Seeed-Studio/OSHW-SenseCAP-Watcher), [SenseCAP-Watcher-Firmware](https://github.com/Seeed-Studio/SenseCAP-Watcher-Firmware), [xiaozhi-esp32](https://github.com/78/xiaozhi-esp32)
- Dots: [TechCrunch](https://techcrunch.com/2026/09/29/openai-launches-dots-its-bubbly-agentic-avatar/), [OpenAI Help](https://help.openai.com/en/articles/20001530-getting-started-with-your-dot)
- Multica: [GitHub](https://github.com/multica-ai/multica), [docs](https://multica.ai/docs/cli)
- Muse: [Meta Model API](https://dev.meta.ai/docs/overview), [TechCrunch](https://techcrunch.com/2026/09/29/meta-is-expanding-its-ai-agent-muse-to-small-businesses/)

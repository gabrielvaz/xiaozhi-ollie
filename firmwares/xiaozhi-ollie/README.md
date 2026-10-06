# Xiaozhi Ollie

Firmware para o **SenseCAP Watcher** (Seeed Studio), baseado no [XiaoZhi](https://github.com/78/xiaozhi-esp32) v2.5.0. Ele transforma o Watcher num painel de mesa: um assistente de voz em português, o **Ollie**, e uma **gaveta de apps** operada pela roda. O foco é acompanhar e comandar sessões do Claude Code e do Codex, além do Multica e do Mac mini.

Funciona com o servidor próprio deste repositório (`../../servidor`), não com a nuvem xiaozhi.me.

## O que muda em relação ao XiaoZhi

| | XiaoZhi original | Xiaozhi Ollie |
|---|---|---|
| Idiomas | Interface em vários idiomas; assistente na nuvem do xiaozhi.me | **4 idiomas de ponta a ponta**: português do Brasil, inglês, chinês simplificado e espanhol (tela, apps, avisos, voz e respostas). Escolha no build: `compilar.sh --idioma en-US` (padrão: `IDIOMA` do `servidor/.env`). Textos do firmware em `TR(pt, en, zh, es)` (`placa/idioma_watcher.h`); em chinês o texto usa a fonte CJK do tema |
| Ativação | "你好小智" (WakeNet chinês) | **"Hey Ollie"** (MultiNet7 em inglês; fonemas gerados no aparelho). O nome do agente é escolhido em Configurações: Ollie, Clawd, Jarvis, Nova, Atlas, Luna, Max ou Iris, e muda a ativação e a apresentação |
| Servidor | xiaozhi.me | **Servidor próprio no Mac** (endereço OTA embutido, Tailscale Funnel com caminho secreto) |
| Mascote | Emojis Noto | **Clawd**, o mascote do Claude Code, em GIFs animados, com animações próprias para **conectando, ouvindo e falando**, **offline** (sem Wi-Fi ou sem servidor, inclusive no boot), **lupa** (procurando atualização), **skate** (baixando atualização), **chateado** (algo falhou), **triste** (bateria no fim), **cozinhando**, **correndo**, **lendo**, **tagarelando** e **xingando** |
| Fala na tela | Uma linha rolando de lado | **Streaming em até 3 linhas**, que rola para cima, com fonte própria de 24 px com acentos |
| Abertura | Logo do XiaoZhi | **Clawd acenando, logo "Ollie" e versão** por ~3 s |
| Fonte | Noto Sans | **Noto Sans ou JetBrains Mono** (estilo terminal), escolhida em Configurações |
| Ao ligar | Logo XiaoZhi | **Logo do Ollie** e depois uma **saudação com o seu nome** ("Bom dia, Ana! Café já tomado?"); em espera, uma **frase aleatória nova a cada 3 min** abaixo do Clawd |
| Tela inicial | Hora | **Dia da semana, data e hora** ("Seg, 05/10 · 16:20") |
| Tela apagada | Só apaga a luz | **Economiza bateria**: animações pausadas, processador com frequência automática (40 a 240 MHz), avisos a cada 5 min |
| Tema | Claro | **Escuro** por padrão (ajustável) |
| Roda | Volume e conversa | Volume (com **anel branco na borda** do tamanho do volume e o Clawd animado), conversa e **gaveta de apps em mosaico** (2 cliques abrem; 2 cliques voltam; 3 cliques fecham) |
| Sem Wi-Fi | Alerta com engrenagem e a dica numa frase | **Tela própria**: Clawd confuso com o Wi-Fi riscado, a rede **Ollie-XXXX** e o endereço do portal; o portal diz **por que** a rede não conectou (só 2,4 GHz, senha recusada, Wi-Fi corporativo, nome com maiúsculas diferentes) |
| Reset de fábrica | Segurar 10 s | **Segurar 20 s** |
| microSD | Não usado | **Registro das conversas, cópia das reuniões e memória offline** |
| Avisos | — | **Avisos do Mac na tela**: sessão esperando você, tarefa concluída, reunião pronta |

## Controles

| Ação | Resultado |
|---|---|
| "Hey Ollie" ou 1 clique | Conversar (clique de novo interrompe). Com a tela apagada, o primeiro clique só acende a tela |
| **2 cliques** | Abre a gaveta de apps; com ela aberta, volta (tela anterior do app, gaveta, ou fecha) |
| **3 cliques** | Fecha a gaveta de qualquer tela |
| Girar | Fora da gaveta: volume. Na gaveta: navega |
| Segurar 2 s (fora do carregador) | Desliga |
| Segurar 20 s | Configurações de fábrica |

## Apps da gaveta

| App | O que faz |
|---|---|
| **Sessões do Claude Code** | Voltar, **Resumir todas** (o Ollie fala o estado de cada sessão), e a lista de sessões do herdr com situação (trabalhando, esperando você, subagentes, concluída). Abrir uma sessão mostra a última mensagem (rolável) e o botão **Enviar pedido** por voz |
| **Conversas** | Todas as conversas com o agente, com título e resumo; abrir mostra as falas (rolável) e **Ouvir** narra a conversa na voz do agente |
| **Uso do Claude** | Arcos com o uso das janelas de 5 h e da semana e o tempo até reiniciar (lido no Mac; o token não vem ao aparelho) |
| **Previsão do tempo** | Tela sem IA: ícones de sol, nuvem, chuva e tempestade; agora, hoje e amanhã (mínima, máxima, chance de chuva); botão **Ollie, fala** |
| **Reuniões** | Lista as reuniões (quando e duração) e grava uma nova, com pausar e continuar. Áudio vai ao Mac (transcrição, resumo, decisões, próximos passos, nota no Apple Notes); cópia no microSD; se a internet cair, continua gravando no cartão |
| **Cronômetro** | Com milissegundos; continua correndo com a gaveta fechada |
| **Contagem regressiva** | Cada passo da roda vale 30 s; alarme com som ao terminar |
| **Relógio mundial** | Aro com 24 bolinhas, uma por fuso (UTC+0 no topo); girar anda o cursor pelo aro e Brasília tem um anel. Cada fuso tem uma cidade (Londres, Nova York, Tóquio, Auckland...), com horário de verão dos EUA, da Europa, da Austrália e da Nova Zelândia |
| **Mostrar QR code** | Wi-Fi atual e a lista de `iCloud Drive/Watcher/QR.md` |
| **Memória offline** | Sessões, últimas reuniões e lembretes, com texto e áudio, guardados no microSD para usar sem internet |
| **Backup** | Conversas e reuniões do microSD para `iCloud Drive/Watcher/Do cartão` (spinner e confirmação) |
| **Configurações** | Nome do agente (reinicia para trocar a ativação), **economia de energia** (brilho 30%, sem "Hey Ollie", pulso a cada 150 s, avisos a cada 1 min com a tela acesa e 10 min apagada), ouvir "Hey Ollie" (desligado, o microfone não fica ouvindo; conversa pela roda), tema, fonte (Noto Sans ou JetBrains Mono), tela apaga após, brilho, volume, desligar na bateria, avisos na tela, Sobre |

Para criar um app: veja [`placa/README.md`](placa/README.md). Cada app é um arquivo em `placa/apps/` mais uma linha em `placa/registro_apps.h`.

## Estrutura

| Caminho | Conteúdo |
|---|---|
| `compilar.sh` | Baixa o XiaoZhi v2.5.0 e o esp-wifi-connect 3.3.1, aplica os patches e compila (ESP-IDF v6.1) |
| `aplicar_patches.py` | Todas as mudanças no código original, aplicadas sempre sobre o código limpo |
| `placa/` | Plataforma de apps, telas, microSD e os apps |
| `mascote-clawd/` | `gerar.py` desenha o Clawd; `previa.png` |
| `placa/layout_mascote.h` | Onde ficam o Clawd e o texto: o corpo do Clawd sempre no centro exato da tela, e o texto logo abaixo dele |
| `simulador/` | Tela do Watcher no Mac (LVGL 9.5 e decodificador de GIF do firmware, mesmo layout). `./rodar.sh --abrir` gera as cenas (abertura, carregando, espera, saudação, trabalhando, ouvindo, falando, volume e as telas de carregamento dos apps), confere se o Clawd está centrado e abre `saida/folha.png` e `saida/poses.png`. Precisa de um `compilar.sh` antes (usa os componentes baixados) |
| `fonte/` | Noto Sans 24 px, JetBrains Mono (opção nas Configurações) e o logo da abertura; como gerar em `fonte/README.md` |
| `../gravar-xiaozhi.sh` | Primeira gravação, com backup e sem tocar na partição de fábrica |
| `../atualizar-firmware.sh` | Atualizações seguintes (mantém o Wi-Fi) |

## Requisitos

- ESP-IDF v6.1 em `~/esp/esp-idf-v6.1` (ou outro caminho em `local.env`, fora do git; veja `../../docs/INSTALL.md`)
- Servidor do repositório rodando no Mac, com o Funnel ligado (o endereço OTA vem de `servidor/.env`)
- microSD em **FAT32 com MBR** (o ESP-IDF não lê exFAT nem GPT)

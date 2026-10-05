# Installing Xiaozhi Ollie

This guide takes you from a SenseCAP Watcher fresh out of the box to saying “Hey Ollie” and hearing about your agent sessions. Plan for about an hour, most of it waiting for builds and downloads.

There are two parts:

1. **The server**, which runs on your Mac. It hears you, thinks with a language model, speaks back and runs the agent tools.
2. **The firmware**, which you build on the Mac and flash to the Watcher over USB-C.

You can follow the steps by hand, or hand them to a coding agent with the prompt in [step 1](#step-1-optional-let-a-coding-agent-do-it).

## What you need

**Hardware**

- A SenseCAP Watcher. The “for XiaoZhi” edition is the closest match, but any Watcher works.
- A USB-C data cable (some cables only charge).
- Optional: a microSD card, **FAT32 with MBR**. ESP-IDF cannot read exFAT or GPT.

**On the Mac** (Apple Silicon, macOS 14 or later)

| Tool | Why | Install |
|---|---|---|
| Homebrew | Installs the rest | [brew.sh](https://brew.sh) |
| uv | Python environments | `brew install uv` |
| ffmpeg | Audio for meetings and offline memory | `brew install ffmpeg` |
| Tailscale, with Funnel allowed | Lets the Watcher reach your Mac from anywhere | [tailscale.com/download](https://tailscale.com/download) |
| ESP-IDF v6.1 | Builds the firmware | see [step 5](#step-5-install-esp-idf) |
| An OpenAI-compatible API key | Speech-to-text, the model and text-to-speech | [OpenRouter](https://openrouter.ai/keys) works out of the box |
| Claude Code, Codex, herdr | The agents Ollie keeps an eye on | each one's own installer; all optional |
| Multica CLI | Issues and autopilots | optional |

## Step 1 (optional): let a coding agent do it

Open a terminal in the repository folder, start **Claude Code**, **Codex** or another coding agent, and paste this prompt. The agent does the setup with you and stops before anything that needs your hands or your approval.

```text
You are helping me install Xiaozhi Ollie from this repository on my Mac.
Read README.md and docs/INSTALL.md first, then follow docs/INSTALL.md step by step.

Rules:
- Before each step, tell me in one sentence what you are about to do. Run the commands yourself
  when you can, and check the result before moving on.
- Check prerequisites first (uv, ffmpeg, tailscale, ESP-IDF v6.1, and optionally claude, codex,
  herdr, multica). If something is missing, show me the install command and ask before running it.
- Secrets: never print, log or commit the contents of servidor/.env. When the API key is needed,
  ask me to paste it into servidor/.env myself, then just confirm the line is filled.
- Ask me for HOST_PUBLICO (my Tailscale machine name, ending in .ts.net), NOME_USUARIO and
  PONTE_RAIZES (the folders where my code projects live) and write them to servidor/.env.
- Stop and wait for me before: turning on Tailscale Funnel, installing the background service,
  flashing the Watcher, and any command that erases data.
- To flash, ask me to connect the Watcher with the USB-C port on the bottom. Use
  firmwares/gravar-xiaozhi.sh for the first flash and firmwares/atualizar-firmware.sh for updates.
- Never commit anything in firmwares/backups/: those files contain my Wi-Fi passwords.
- If a command fails, read the error, explain it in plain words and propose a fix before retrying.
- At the end, run servidor/verificar.sh, show me the summary, and tell me what to say to the
  Watcher to test it.
```

## Step 2: get the code

```sh
git clone https://github.com/gabrielvaz/xiaozhi-ollie.git
cd xiaozhi-ollie
```

Everything below runs from this folder.

## Step 3: set up the server

```sh
servidor/instalar.sh
```

This downloads the xiaozhi-esp32-server at a tested commit, applies the Portuguese translations and fixes, creates the Python environment and writes `servidor/.env` with a fresh secret path and signing key.

Now open `servidor/.env` and fill in:

| Variable | What to put |
|---|---|
| `API_KEY` | Your OpenRouter (or other OpenAI-compatible) key |
| `HOST_PUBLICO` | Your Mac's Tailscale name, such as `my-mac.tail1234.ts.net` (no `https://`). Find it with `tailscale status` or in the Tailscale admin console |
| `NOME_USUARIO` | Your first name, used in greetings |
| `PONTE_RAIZES` | Folders where your projects live, separated by `:` |
| `WATCHER_VOCABULARIO` | Optional: names the meeting transcription should spell right |

Leave `SEGREDO` and `AUTH_KEY` as generated. The models and the voice have working defaults.

Test the provider with real calls:

```sh
servidor/testar-api.sh
```

It runs a chat with a tool call, a voice sample and a transcription. Fix any error here before going on.

## Step 4: open the door with Tailscale Funnel

The Watcher needs to reach your Mac from home Wi-Fi or from your phone's hotspot. Funnel publishes **only** the secret path from `.env`; everything else stays closed.

```sh
servidor/configurar-funnel.sh ligar
```

The first time, Tailscale may ask you to allow Funnel for your tailnet in the browser. The script prints the OTA URL; the firmware build in step 6 embeds it for you.

Then install the server as a background service that starts at login:

```sh
servidor/instalar-servico.sh instalar
servidor/instalar-servico.sh log      # follow the log; Ctrl+C to stop following
```

## Step 5: install ESP-IDF

Skip this if you already have ESP-IDF v6.1.

```sh
mkdir -p ~/esp
git clone -b v6.1 --recursive https://github.com/espressif/esp-idf.git ~/esp/esp-idf-v6.1
~/esp/esp-idf-v6.1/install.sh esp32s3
```

The build looks in `~/esp/esp-idf-v6.1` by default. If yours lives elsewhere, create `firmwares/xiaozhi-ollie/local.env` (it is ignored by git):

```sh
export IDF_PATH=/path/to/esp-idf-v6.1
export IDF_TOOLS_PATH=/path/to/.espressif
```

## Step 6: build the firmware

```sh
firmwares/xiaozhi-ollie/compilar.sh
```

It downloads XiaoZhi v2.5.0, applies the Ollie patches, embeds your server address from `servidor/.env` and builds. The first build takes several minutes. The result is `firmwares/xiaozhi-ollie/saida/merged-binary.bin`.

## Step 7: flash the Watcher

Connect the Watcher with the **USB-C port on the bottom** (the one on the back only charges). The Mac sees two serial ports; the script finds the ESP32-S3 on its own.

```sh
firmwares/gravar-xiaozhi.sh
```

On the first run it saves a **full 32 MB backup** of the flash to `firmwares/backups/`, then writes the firmware without touching the factory partition and checks it afterwards. Keep that backup somewhere safe and private: it contains your Wi-Fi passwords. It is all you need to go back to the original firmware.

For later updates, use the script that keeps Wi-Fi and settings:

```sh
firmwares/atualizar-firmware.sh
```

## Step 8: connect the Watcher to Wi-Fi

1. After flashing, the Watcher creates a network called `Xiaozhi-XXXX`.
2. Join it from your phone; the setup page opens (or browse to the address shown on the screen).
3. Add your home Wi-Fi. Add your phone's hotspot too, so Ollie works away from home. On an iPhone, turn on **Maximize Compatibility**: the Watcher only uses 2.4 GHz.
4. The server address is already built in. The **Advanced** tab is only for pointing to a different server.

## Step 9: check everything

```sh
servidor/verificar.sh
```

It checks the API key and models, herdr, Claude Code, Codex, Multica, the local server and the public Funnel path, and that the public root stays closed.

Then say **“Hey Ollie”**, or click the wheel, and ask how your sessions are doing.

## Optional extras

**Personal files in iCloud Drive.** Ollie reads and writes a `Watcher` folder in iCloud Drive:

| Path | Use |
|---|---|
| `Watcher/Memória/*.md` | Notes Ollie always knows. Start with `sobre-mim.md`: who you are, your projects, how you like answers |
| `Watcher/Lembretes.md` | Reminders saved by voice |
| `Watcher/QR.md` | Extra entries for the QR code app |
| `Watcher/Reuniões/` | Meeting transcripts and summaries |

**Multica.** Run `multica login` and `multica daemon start` to use the issue and autopilot tools.

**Other XiaoZhi devices.** Any device running XiaoZhi v2 can use your Ollie server for voice and the agent tools. Paste the OTA URL printed by `configurar-funnel.sh` into the **Custom OTA URL** field of its Wi-Fi setup page.

## Troubleshooting

| Problem | What to try |
|---|---|
| The flash script does not find the Watcher | Use the bottom USB-C port and a data cable. Pass the port by hand: `firmwares/gravar-xiaozhi.sh /dev/cu.usbmodemXXXX` |
| The Watcher says it cannot reach the server | Run `servidor/verificar.sh`. Check that the service is running (`servidor/instalar-servico.sh log`) and that Funnel is on |
| Voice works but the agent tools fail | Check `PONTE_RAIZES` in `servidor/.env` and that `claude`, `codex` and `herdr` run in a normal terminal |
| The build stops with “region overflowed” | The app partition is full. Remove an app from `firmwares/xiaozhi-ollie/placa/registro_apps.h` or move fonts to the assets partition |
| You want the original firmware back | Flash the full backup from `firmwares/backups/` with esptool |

"""Gravador de reuniões do Watcher.

Durante a reunião o Watcher manda o áudio pelo mesmo canal da voz; o servidor grava o PCM
(16 kHz, mono) num WAV. Ao parar, em segundo plano:
  1. converte para audio.m4a (AAC) e apaga o WAV;
  2. transcreve em partes de 10 min (OpenRouter, MODELO_ASR_REUNIAO);
  3. gera resumo, decisões e próximos passos (MODELO_RESUMO);
  4. cria uma nota no Apple Notes (pasta "Reuniões Watcher") e avisa no Mac.
Pasta: iCloud Drive/Watcher/Reuniões/AAAA-MM-DD HHhMM/ (ou WATCHER_PASTA_REUNIOES).
"""

import html
import json
import os
import subprocess
import tempfile
import threading
import wave
from datetime import datetime
from pathlib import Path

import requests

from core.utils.idioma import CODIGO, NOME, t

PASTA = Path(os.environ.get(
    "WATCHER_PASTA_REUNIOES",
    Path.home() / "Library/Mobile Documents/com~apple~CloudDocs/Watcher/Reuniões",
))
TAXA = 16000
MINUTOS_POR_PARTE = 10
FFMPEG = "/opt/homebrew/bin/ffmpeg"
# Nomes próprios que o modelo de transcrição deve reconhecer (ajuste em WATCHER_VOCABULARIO)
VOCABULARIO = os.environ.get("WATCHER_VOCABULARIO", "Claude Code, Codex, herdr, Multica")

PROMPT_RESUMO = f"""Você recebe a transcrição de uma reunião gravada pelo usuário num SenseCAP Watcher.
Escreva em {NOME}, em Markdown, com exatamente estes títulos:
## {t('Resumo', 'Summary', '摘要', 'Resumen')}
3 a 6 frases com o essencial.
## {t('Decisões', 'Decisions', '决定', 'Decisiones')}
Lista curta; "{t('Nenhuma registrada', 'None recorded', '无记录', 'Ninguna registrada')}" se não houver.
## {t('Próximos passos', 'Next steps', '后续步骤', 'Próximos pasos')}
Lista com responsável e prazo quando aparecerem na conversa.
## {t('Pontos em aberto', 'Open questions', '待解决问题', 'Puntos pendientes')}
Dúvidas ou riscos citados.
Não invente nomes, números ou prazos que não estejam na transcrição."""


MESES_EN = ["Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"]


def _data_hora(d: datetime) -> str:
    """Data e hora no formato natural do idioma (pt: 05/10/2026 14:30)."""
    return t(f"{d:%d/%m/%Y %H:%M}", f"{MESES_EN[d.month - 1]} {d.day}, {d.year} {d:%H:%M}",
             f"{d.year}年{d.month}月{d.day}日 {d:%H:%M}", f"{d.day}/{d.month}/{d.year} {d:%H:%M}")


class GravadorReuniao:
    def __init__(self, logger=None, inicio: datetime | None = None):
        self.inicio = inicio or datetime.now()
        self.pasta = PASTA / self.inicio.strftime("%Y-%m-%d %Hh%M")
        self.pasta.mkdir(parents=True, exist_ok=True)
        self.caminho_wav = self.pasta / "audio.wav"
        self._wav = wave.open(str(self.caminho_wav), "wb")
        self._wav.setnchannels(1)
        self._wav.setsampwidth(2)
        self._wav.setframerate(TAXA)
        self._trava = threading.Lock()
        self.amostras = 0
        self.pausado = False
        self.logger = logger

    def adicionar(self, pcm: bytes) -> None:
        # Em pausa o aparelho continua mandando áudio (mantém a conexão viva); os quadros são descartados.
        if not pcm or self.pausado:
            return
        with self._trava:
            if self._wav is not None and not self.pausado:
                self._wav.writeframes(pcm)
                self.amostras += len(pcm) // 2

    def pausar(self) -> None:
        self.pausado = True

    def continuar(self) -> None:
        self.pausado = False

    @property
    def minutos(self) -> float:
        return self.amostras / TAXA / 60

    def parar(self, processar: bool = True) -> str:
        with self._trava:
            if self._wav is None:
                return ""
            self._wav.close()
            self._wav = None
        gravar_info(self.pasta, self.inicio, self.amostras / TAXA)
        if processar:
            threading.Thread(target=self._processar, daemon=True).start()
        return f"{self.minutos:.0f} min"

    # ------------------------------------------------------------ processamento

    def _log(self, msg):
        if self.logger:
            self.logger.info(f"[reuniao] {msg}")
        with open(self.pasta / "processamento.log", "a", encoding="utf-8") as f:
            f.write(f"{datetime.now():%H:%M:%S} {msg}\n")

    def _processar(self):
        try:
            if self.amostras < TAXA * 5:
                self._log("gravação com menos de 5 s; descartada")
                return
            m4a = self.pasta / "audio.m4a"
            subprocess.run([FFMPEG, "-y", "-loglevel", "error", "-i", str(self.caminho_wav),
                            "-c:a", "aac", "-b:a", "48k", str(m4a)], check=True)
            self._log(f"áudio salvo ({self.minutos:.1f} min)")
            transcricao = self._transcrever()
            self.caminho_wav.unlink(missing_ok=True)
            titulo = t("Reunião", "Meeting", "会议", "Reunión") + f" {_data_hora(self.inicio)} ({self.minutos:.0f} min)"
            (self.pasta / "transcricao.md").write_text(f"# {titulo}\n\n{transcricao}\n", encoding="utf-8")
            resumo = self._resumir(transcricao)
            (self.pasta / "resumo.md").write_text(f"# {titulo}\n\n{resumo}\n", encoding="utf-8")
            self._log("transcrição e resumo prontos")
            self._nota_apple(titulo, resumo, transcricao)
            self._notificar(f"{titulo}: " + t("resumo pronto no Notas e no iCloud.", "summary ready in Notes and iCloud.",
                                               "摘要已保存到备忘录和 iCloud。", "resumen listo en Notas y en iCloud."))
            from core.utils.vigia import adicionar_aviso
            minutos = f"{self.minutos:.0f}"
            adicionar_aviso("reuniao", t("Reunião pronta", "Meeting ready", "会议已就绪", "Reunión lista"),
                            t(f"Resumo de {minutos} min salvo no Notas", f"{minutos}-min summary saved to Notes",
                              f"{minutos} 分钟的摘要已保存到备忘录", f"Resumen de {minutos} min guardado en Notas"), "happy")
        except Exception as e:
            self._log(f"ERRO: {e}")
            self._notificar(t("Reunião gravada, mas o processamento falhou", "Meeting recorded, but processing failed",
                             "会议已录制，但处理失败", "Reunión grabada, pero el procesamiento falló") + f": {e}")

    def _transcrever(self) -> str:
        base = os.environ.get("API_BASE_URL", "https://openrouter.ai/api/v1")
        chave = os.environ.get("API_KEY", "")
        modelo = os.environ.get("MODELO_ASR_REUNIAO", "openai/gpt-4o-transcribe")
        partes_txt = []
        with tempfile.TemporaryDirectory() as tmp:
            subprocess.run([FFMPEG, "-y", "-loglevel", "error", "-i", str(self.caminho_wav), "-f", "segment",
                            "-segment_time", str(MINUTOS_POR_PARTE * 60), "-c:a", "libmp3lame", "-b:a", "48k",
                            f"{tmp}/parte_%03d.mp3"], check=True)
            for i, parte in enumerate(sorted(Path(tmp).glob("parte_*.mp3"))):
                with open(parte, "rb") as f:
                    r = requests.post(f"{base}/audio/transcriptions", headers={"Authorization": f"Bearer {chave}"},
                                      data={"model": modelo, "language": CODIGO, "prompt": t("Reunião em português. Termos", "Meeting in English. Terms",
                                                                              "中文会议。术语", "Reunión en español. Términos") + f": {VOCABULARIO}."}, files={"file": f}, timeout=600)
                r.raise_for_status()
                inicio = i * MINUTOS_POR_PARTE
                partes_txt.append(f"**[{inicio // 60:02d}:{inicio % 60:02d}]** {r.json().get('text', '').strip()}")
                self._log(f"parte {i + 1} transcrita")
        return "\n\n".join(partes_txt)

    def _resumir(self, transcricao: str) -> str:
        base = os.environ.get("API_BASE_URL", "https://openrouter.ai/api/v1")
        r = requests.post(f"{base}/chat/completions", timeout=600,
                          headers={"Authorization": f"Bearer {os.environ.get('API_KEY', '')}"},
                          json={"model": os.environ.get("MODELO_RESUMO", "openai/gpt-6.1-sol"),
                                "messages": [{"role": "system", "content": PROMPT_RESUMO + f"\nNomes corretos: {VOCABULARIO}. "
                                              "Se a transcrição trouxer uma grafia parecida (ex.: Vireio), use a correta."},
                                             {"role": "user", "content": transcricao}]})
        r.raise_for_status()
        return r.json()["choices"][0]["message"]["content"].strip()

    def _nota_apple(self, titulo: str, resumo: str, transcricao: str):
        def bloco(md):
            linhas = []
            for l in md.splitlines():
                if l.startswith("## "):
                    linhas.append(f"<h2>{html.escape(l[3:])}</h2>")
                elif l.strip():
                    linhas.append(f"<div>{html.escape(l)}</div>")
            return "".join(linhas)
        corpo = (f"<h1>{html.escape(titulo)}</h1>{bloco(resumo)}"
                 f"<h2>{t('Transcrição', 'Transcript', '转写', 'Transcripción')}</h2>{bloco(transcricao)}"
                 f"<div><br>{t('Arquivos', 'Files', '文件', 'Archivos')}: {html.escape(str(self.pasta))}</div>")
        arq = self.pasta / ".nota.html"
        arq.write_text(corpo, encoding="utf-8")
        script = f'''
set corpo to read POSIX file "{arq}" as «class utf8»
tell application "Notes"
    if not (exists folder "Reuniões Watcher") then make new folder with properties {{name:"Reuniões Watcher"}}
    make new note at folder "Reuniões Watcher" with properties {{body:corpo}}
end tell'''
        r = subprocess.run(["osascript", "-e", script], capture_output=True, text=True, timeout=60)
        arq.unlink(missing_ok=True)
        self._log("nota criada no Apple Notes" if r.returncode == 0 else f"Notas falhou: {r.stderr.strip()}")

    def _notificar(self, msg: str):
        msg = msg.replace('"', "'")[:200]
        subprocess.run(["osascript", "-e", f'display notification "{msg}" with title "Watcher: {t("reunião", "meeting", "会议", "reunión")}"'],
                       capture_output=True, timeout=15)


def gravar_info(pasta: Path, inicio: datetime, duracao_s: float) -> None:
    """info.json da reunião: {"inicio": iso, "duracao_s": int} (duração só do tempo gravado)."""
    try:
        (pasta / "info.json").write_text(json.dumps({"inicio": inicio.isoformat(timespec="seconds"),
                                                     "duracao_s": int(round(duracao_s))}), encoding="utf-8")
    except OSError:
        pass


def processar_backup(caminho: Path, logger=None) -> str:
    """Reunião que veio do microSD (arquivo .wopus: cabeçalho + [u16 tamanho][pacote Opus]...).

    Se a mesma reunião já foi processada pela internet (mesma pasta com resumo.md), só arquiva.
    """
    import struct

    import opuslib_next

    dados = caminho.read_bytes()
    if not dados.startswith(b"WOPUS1\n"):
        return "formato desconhecido"
    try:
        inicio = datetime.strptime(caminho.stem.split(".")[0], "%Y%m%d-%H%M%S")
    except ValueError:
        inicio = datetime.fromtimestamp(caminho.stat().st_mtime)
    pasta = PASTA / inicio.strftime("%Y-%m-%d %Hh%M")
    if (pasta / "resumo.md").exists():
        if not (pasta / "info.json").exists():
            # Conta os pacotes (60 ms = 960 amostras cada) para registrar a duração.
            pacotes, pos = 0, len(b"WOPUS1\n")
            while pos + 2 <= len(dados):
                (tamanho,) = struct.unpack_from("<H", dados, pos)
                pos += 2 + tamanho
                if tamanho and pos <= len(dados):
                    pacotes += 1
            if pacotes:
                gravar_info(pasta, inicio, pacotes * 960 / TAXA)
        return "já processada pela internet"
    gravador = GravadorReuniao(logger, inicio=inicio)
    decodificador = opuslib_next.Decoder(TAXA, 1)
    pos = len(b"WOPUS1\n")
    while pos + 2 <= len(dados):
        (tamanho,) = struct.unpack_from("<H", dados, pos)
        pos += 2
        pacote = dados[pos:pos + tamanho]
        pos += tamanho
        if len(pacote) == tamanho and tamanho:
            try:
                gravador.adicionar(decodificador.decode(pacote, 960))
            except Exception:
                pass
    gravador._log(f"reunião recebida do microSD ({caminho.name})")
    return f"processando {gravador.parar()}"

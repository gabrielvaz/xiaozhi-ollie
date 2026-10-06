"""Registro de todas as conversas com o Ollie, organizado para servir de memória.

Grava numa pasta LOCAL do servidor (xiaozhi-server/data/conversas), sem esperar disco nem rede: a
conversa só põe a fala numa fila. A cópia para o iCloud Drive/Watcher/Conversas (ou
WATCHER_PASTA_CONVERSAS) só acontece quando o app Backup do Watcher roda (POST /watcher/backup).
  AAAA/MM/AAAA-MM-DD HHhMM.md   uma conversa por arquivo (nova conexão ou 10 min de silêncio)
  indice.json                   id, arquivo, início, fim, falas, título e resumo de cada conversa

Chamado por Dialogue.put (patch em patches/traduzir_servidor.py). Um fio em segundo plano dá
título e resumo às conversas encerradas; os resumos recentes entram no bloco <memoria> do prompt.
Nunca derruba a conversa.
"""

import errno
import json
import os
import queue
import threading
import time
from datetime import datetime, timedelta
from pathlib import Path

import requests

from core.utils.icloud import ler_texto
from core.utils.idioma import IDIOMA, NOME, t

PASTA = Path(__file__).resolve().parents[2] / "data/conversas"   # local: rápido, a conversa nunca espera o iCloud
ESPELHO = Path(os.environ.get(
    "WATCHER_PASTA_CONVERSAS",
    Path.home() / "Library/Mobile Documents/com~apple~CloudDocs/Watcher/Conversas",
))
MARCA_ESPELHO = PASTA / ".ultimo_backup"   # quando foi a última cópia para o iCloud
INDICE = PASTA / "indice.json"
LIMITE_FERRAMENTA = 600
SILENCIO_NOVA_S = 600        # 10 min sem falas: próxima fala abre outra conversa
ENCERRADA_S = 300            # 5 min sem falas: a conversa ganha título e resumo
DIAS = {
    "pt-BR": ["Seg", "Ter", "Qua", "Qui", "Sex", "Sáb", "Dom"],
    "en-US": ["Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"],
    "zh-CN": ["周一", "周二", "周三", "周四", "周五", "周六", "周日"],
    "es-ES": ["lun", "mar", "mié", "jue", "vie", "sáb", "dom"],
}[IDIOMA]
MESES_EN = ["Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"]
# Rótulo do usuário nas falas gravadas; falas() aceita o de qualquer idioma e devolve sempre "Você"
VOCE = t("Você", "You", "你", "Tú")
ROTULOS_USUARIO = ("Você", "You", "你", "Tú")
_trava = threading.RLock()
_fio = {"iniciado": False}


def _data(d: datetime, hora: bool = True) -> str:
    """Data do cabeçalho no formato natural do idioma (pt: Seg, 05/10/2026 às 14:30)."""
    dia = DIAS[d.weekday()]
    texto = t(f"{dia}, {d:%d/%m/%Y}", f"{dia}, {MESES_EN[d.month - 1]} {d.day}, {d.year}",
              f"{d.year}年{d.month}月{d.day}日 {dia}", f"{dia}, {d.day}/{d.month}/{d.year}")
    if hora:
        texto += t(f" às {d:%H:%M}", f" at {d:%H:%M}", f" {d:%H:%M}", f" a las {d:%H:%M}")
    return texto


# ---------------------------------------------------------------- índice

def _ler_indice() -> list[dict]:
    try:
        return json.loads(ler_texto(INDICE, padrao="[]") or "[]")
    except (json.JSONDecodeError, OSError):
        return []


def _gravar_indice(itens: list[dict]) -> None:
    PASTA.mkdir(parents=True, exist_ok=True)
    tmp = INDICE.with_suffix(".tmp")
    tmp.write_text(json.dumps(itens, ensure_ascii=False, indent=1), encoding="utf-8")
    tmp.replace(INDICE)


def conversas(limite: int = 50) -> list[dict]:
    """Conversas mais recentes primeiro."""
    with _trava:
        return sorted(_ler_indice(), key=lambda c: c["inicio"], reverse=True)[:limite]


def conversa(cid: str) -> dict | None:
    with _trava:
        return next((c for c in _ler_indice() if c["id"] == cid), None)


def falas(c: dict) -> list[tuple[str, str]]:
    """[(quem, texto)] só com o que foi dito (sem ferramentas)."""
    saida = []
    for linha in ler_texto(PASTA / c["arquivo"], padrao="").splitlines():
        if linha.startswith("**") and ":** " in linha:
            cabeca, texto = linha[2:].split(":** ", 1)
            quem = cabeca.split(" · ", 1)[-1]
            saida.append(("Você" if quem in ROTULOS_USUARIO else "Ollie", texto.strip()))
    return saida


# ---------------------------------------------------------------- registro

def _texto_usuario(texto: str) -> str:
    if texto.startswith("{") and '"content"' in texto:
        try:
            return json.loads(texto).get("content", texto)
        except json.JSONDecodeError:
            pass
    return texto


def _linha(message, agora: datetime) -> str | None:
    hora = agora.strftime("%H:%M:%S")
    papel = getattr(message, "role", "")
    texto = (getattr(message, "content", None) or "").strip()
    if papel == "user" and texto:
        return f"**{hora} · {VOCE}:** {_texto_usuario(texto)}"
    if papel == "assistant":
        chamadas = getattr(message, "tool_calls", None)
        if chamadas:
            nomes = []
            for c in chamadas:
                f = c.get("function", {}) if isinstance(c, dict) else {}
                nomes.append(f"`{f.get('name', '?')}({f.get('arguments', '')})`")
            return f"{hora} · 🔧 {', '.join(nomes)}"
        if texto:
            from core.utils.perfil import nome_agente
            return f"**{hora} · {nome_agente()}:** {texto}"
    if papel == "tool" and texto:
        resumo = texto if len(texto) <= LIMITE_FERRAMENTA else texto[:LIMITE_FERRAMENTA] + "…"
        return f"{hora} · ↳ <sub>{json.dumps(resumo, ensure_ascii=False)[1:-1]}</sub>"
    return None


def _nova_conversa(agora: datetime) -> dict:
    cid = agora.strftime("%Y%m%d-%H%M%S")
    arquivo = f"{agora:%Y/%m}/{agora:%Y-%m-%d %Hh%M}.md"
    if (PASTA / arquivo).exists():  # outra conversa no mesmo minuto
        arquivo = f"{agora:%Y/%m}/{agora:%Y-%m-%d %Hh%Mm%S}.md"
    (PASTA / arquivo).parent.mkdir(parents=True, exist_ok=True)
    with open(PASTA / arquivo, "a", encoding="utf-8") as f:
        f.write(f"# {t('Conversa', 'Conversation', '对话', 'Conversación')} · {_data(agora)}\n\n")
    return {"id": cid, "arquivo": arquivo, "inicio": agora.isoformat(timespec="seconds"),
            "fim": agora.isoformat(timespec="seconds"), "falas": 0, "titulo": "", "resumo": ""}


_fila: "queue.Queue" = queue.Queue()


def registrar(message, dono=None) -> None:
    """dono: o Dialogue da conexão (uma conexão nova abre uma conversa nova). Só enfileira: quem grava é
    um fio em segundo plano, então a conversa nunca espera o disco."""
    try:
        if getattr(message, "is_temporary", False) or getattr(message, "role", "") == "system":
            return
        agora = datetime.now()
        linha = _linha(message, agora)
        if not linha:
            return
        _iniciar_fio()
        _fila.put((linha, dono, agora))
    except Exception:
        pass


def _gravador() -> None:
    while True:
        linha, dono, agora = _fila.get()
        try:
            _gravar(linha, dono, agora)
        except Exception:
            pass


def _gravar(linha: str, dono, agora: datetime) -> None:
    with _trava:
        itens = _ler_indice()
        cid = getattr(dono, "_conversa_watcher", None)
        atual = next((c for c in itens if c["id"] == cid), None) if cid else None
        if atual is None or (agora - datetime.fromisoformat(atual["fim"])).total_seconds() > SILENCIO_NOVA_S:
            atual = _nova_conversa(agora)
            itens.append(atual)
            if dono is not None:
                dono._conversa_watcher = atual["id"]
        with open(PASTA / atual["arquivo"], "a", encoding="utf-8") as f:
            f.write(linha + "\n\n")
        atual["fim"] = agora.isoformat(timespec="seconds")
        if linha.startswith("**"):
            atual["falas"] += 1
        atual["resumo"] = ""  # fala nova: o resumo é refeito quando a conversa encerrar
        _gravar_indice(itens)


# ---------------------------------------------------------------- cópia no iCloud (segundo plano)

def _trazer_do_icloud() -> None:
    """Primeira vez com a pasta local: traz as conversas que já estavam no iCloud."""
    if (PASTA / "indice.json").exists() or not (ESPELHO / "indice.json").exists():
        return
    import shutil
    for origem in ESPELHO.rglob("*"):
        if origem.is_file():
            destino = PASTA / origem.relative_to(ESPELHO)
            destino.parent.mkdir(parents=True, exist_ok=True)
            try:
                shutil.copy2(origem, destino)
            except OSError:
                pass


def espelhar() -> int:
    """Backup: copia para o iCloud o que mudou na pasta local desde o último backup. Devolve quantos arquivos."""
    import shutil
    try:
        desde = float(MARCA_ESPELHO.read_text())
    except (OSError, ValueError):
        desde = 0.0
    inicio = time.time()
    copiados = 0
    falhas: list[str] = []
    with _trava:
        for origem in PASTA.rglob("*"):
            if (not origem.is_file() or origem.suffix == ".tmp" or origem.name.startswith(".")
                    or origem.stat().st_mtime < desde):
                continue
            destino = ESPELHO / origem.relative_to(PASTA)
            for tentativa in range(10):  # o iCloud devolve EDEADLK enquanto sincroniza: tenta de novo
                try:
                    destino.parent.mkdir(parents=True, exist_ok=True)
                    shutil.copy2(origem, destino)
                    copiados += 1
                    break
                except OSError as e:
                    if e.errno != errno.EDEADLK or tentativa == 9:
                        falhas.append(origem.name)
                        break
                    time.sleep(0.5)
    if falhas:  # não marca o backup como feito: os que falharam vão na próxima vez
        raise OSError(f"{len(falhas)} arquivo(s) não copiados para o iCloud: {', '.join(falhas[:3])}")
    MARCA_ESPELHO.write_text(str(inicio))
    return copiados


# ---------------------------------------------------------------- título e resumo

def _resumir(c: dict) -> tuple[str, str]:
    texto = "\n".join(f"{q}: {t}" for q, t in falas(c))[:12000]
    if not texto:
        return t("Conversa sem falas", "Conversation without speech", "没有发言的对话", "Conversación sin intervenciones"), ""
    base = os.environ.get("API_BASE_URL", "https://openrouter.ai/api/v1")
    r = requests.post(f"{base}/chat/completions", timeout=60,
                      headers={"Authorization": f"Bearer {os.environ.get('API_KEY', '')}"},
                      json={"model": os.environ.get("MODELO_LLM", "openai/gpt-6-luna"), "max_tokens": 500,
                            "reasoning": {"effort": "minimal"},
                            "response_format": {"type": "json_object"},
                            "messages": [{"role": "system", "content":
                                          "Você organiza o histórico de conversas do usuário com o assistente de voz Ollie. "
                                          "Responda só com JSON: {\"titulo\": até 6 palavras, sem ponto final, "
                                          "\"resumo\": 1 ou 2 frases com o que foi pedido, "
                                          "respondido ou decidido, incluindo nomes, números e fatos úteis para lembrar depois}. "
                                          f"Escreva o título e o resumo em {NOME}."},
                                         {"role": "user", "content": texto}]})
    r.raise_for_status()
    dados = json.loads(r.json()["choices"][0]["message"]["content"])
    return str(dados.get("titulo", "")).strip()[:80] or t("Conversa", "Conversation", "对话", "Conversación"), str(dados.get("resumo", "")).strip()[:600]


def _escrever_resumo_no_arquivo(c: dict) -> None:
    arq = PASTA / c["arquivo"]
    linhas = ler_texto(arq, padrao="").splitlines(keepends=True)
    if not linhas:
        return
    corpo = [l for l in linhas[1:] if not l.startswith("## ") and not l.startswith("> ")]
    while corpo and corpo[0].strip() == "":
        corpo.pop(0)
    arq.write_text(linhas[0] + f"\n## {c['titulo']}\n\n> {c['resumo']}\n\n" + "".join(corpo), encoding="utf-8")


def resumir_pendentes() -> None:
    limite = datetime.now() - timedelta(seconds=ENCERRADA_S)
    with _trava:
        pendentes = [c for c in _ler_indice() if not c.get("resumo") and datetime.fromisoformat(c["fim"]) < limite]
    for c in pendentes:
        try:
            titulo, resumo = _resumir(c)
        except Exception:
            continue
        with _trava:
            itens = _ler_indice()
            alvo = next((x for x in itens if x["id"] == c["id"]), None)
            if alvo is None or alvo["fim"] != c["fim"]:
                continue  # a conversa recebeu falas novas enquanto resumia
            alvo["titulo"], alvo["resumo"] = titulo, resumo or t("Sem conteúdo relevante.", "Nothing relevant.", "没有相关内容。", "Sin contenido relevante.")
            _escrever_resumo_no_arquivo(alvo)
            _gravar_indice(itens)


def _migrar_antigos() -> None:
    """Arquivos do formato antigo (Conversas/AAAA-MM-DD.md, um por dia) viram uma conversa cada."""
    with _trava:
        itens = _ler_indice()
        for arq in sorted(PASTA.glob("????-??-??.md")):
            dia = datetime.strptime(arq.stem, "%Y-%m-%d")
            horas = [l.split(" · ")[0].strip("*") for l in ler_texto(arq, padrao="").splitlines() if " · " in l]
            horas = [h for h in horas if len(h) == 8 and h[2] == ":"]
            inicio = datetime.combine(dia.date(), datetime.strptime(horas[0], "%H:%M:%S").time()) if horas else dia
            fim = datetime.combine(dia.date(), datetime.strptime(horas[-1], "%H:%M:%S").time()) if horas else dia
            destino = f"{dia:%Y/%m}/{dia:%Y-%m-%d} (dia inteiro).md"
            (PASTA / destino).parent.mkdir(parents=True, exist_ok=True)
            texto = ler_texto(arq, padrao="").replace("· Jarvis:**", "· Ollie:**")
            texto = texto.replace(texto.splitlines()[0], f"# {t('Conversas', 'Conversations', '对话', 'Conversaciones')} · {_data(dia, hora=False)}", 1)
            (PASTA / destino).write_text(texto, encoding="utf-8")
            arq.unlink()
            itens.append({"id": inicio.strftime("%Y%m%d-%H%M%S"), "arquivo": destino,
                          "inicio": inicio.isoformat(timespec="seconds"), "fim": fim.isoformat(timespec="seconds"),
                          "falas": texto.count(" · Você:**") + texto.count(" · Ollie:**"), "titulo": "", "resumo": ""})
        _gravar_indice(itens)


def _loop() -> None:
    try:
        _trazer_do_icloud()
        _migrar_antigos()
    except Exception:
        pass
    while True:
        try:
            resumir_pendentes()
        except Exception:
            pass
        time.sleep(60)


def iniciar() -> None:
    _iniciar_fio()


def _iniciar_fio() -> None:
    if not _fio["iniciado"]:
        _fio["iniciado"] = True
        threading.Thread(target=_gravador, daemon=True, name="diario-gravador").start()
        threading.Thread(target=_loop, daemon=True, name="diario-conversas").start()


# ---------------------------------------------------------------- memória

def memoria_recente(limite_chars: int = 1800) -> str:
    """Resumos das últimas conversas, para o bloco <memoria> do prompt."""
    linhas = []
    for c in conversas(20):
        if not c.get("resumo"):
            continue
        quando = datetime.fromisoformat(c["inicio"])
        linhas.append(f"- {quando:%d/%m %H:%M} · {c['titulo']}: {c['resumo']}")
        if sum(len(l) for l in linhas) > limite_chars:
            linhas.pop()
            break
    return ("## Conversas anteriores com o usuário (o que vocês já conversaram; use quando ele perguntar)\n" + "\n".join(linhas)) if linhas else ""

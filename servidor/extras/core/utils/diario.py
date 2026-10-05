"""Registro de todas as conversas com o Ollie, organizado para servir de memória.

Destino: iCloud Drive/Watcher/Conversas (ou WATCHER_PASTA_CONVERSAS)
  AAAA/MM/AAAA-MM-DD HHhMM.md   uma conversa por arquivo (nova conexão ou 10 min de silêncio)
  indice.json                   id, arquivo, início, fim, falas, título e resumo de cada conversa

Chamado por Dialogue.put (patch em patches/traduzir_servidor.py). Um fio em segundo plano dá
título e resumo às conversas encerradas; os resumos recentes entram no bloco <memoria> do prompt.
Nunca derruba a conversa.
"""

import json
import os
import threading
import time
from datetime import datetime, timedelta
from pathlib import Path

import requests

from core.utils.icloud import ler_texto

PASTA = Path(os.environ.get(
    "WATCHER_PASTA_CONVERSAS",
    Path.home() / "Library/Mobile Documents/com~apple~CloudDocs/Watcher/Conversas",
))
INDICE = PASTA / "indice.json"
LIMITE_FERRAMENTA = 600
SILENCIO_NOVA_S = 600        # 10 min sem falas: próxima fala abre outra conversa
ENCERRADA_S = 300            # 5 min sem falas: a conversa ganha título e resumo
DIAS = ["Seg", "Ter", "Qua", "Qui", "Sex", "Sáb", "Dom"]
_trava = threading.RLock()
_fio = {"iniciado": False}


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
            saida.append(("Você" if quem == "Você" else "Ollie", texto.strip()))
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
        return f"**{hora} · Você:** {_texto_usuario(texto)}"
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
        f.write(f"# Conversa · {DIAS[agora.weekday()]}, {agora:%d/%m/%Y} às {agora:%H:%M}\n\n")
    return {"id": cid, "arquivo": arquivo, "inicio": agora.isoformat(timespec="seconds"),
            "fim": agora.isoformat(timespec="seconds"), "falas": 0, "titulo": "", "resumo": ""}


def registrar(message, dono=None) -> None:
    """dono: o Dialogue da conexão (uma conexão nova abre uma conversa nova)."""
    try:
        if getattr(message, "is_temporary", False) or getattr(message, "role", "") == "system":
            return
        agora = datetime.now()
        linha = _linha(message, agora)
        if not linha:
            return
        _iniciar_fio()
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
    except Exception:
        pass


# ---------------------------------------------------------------- título e resumo

def _resumir(c: dict) -> tuple[str, str]:
    texto = "\n".join(f"{q}: {t}" for q, t in falas(c))[:12000]
    if not texto:
        return "Conversa sem falas", ""
    base = os.environ.get("API_BASE_URL", "https://openrouter.ai/api/v1")
    r = requests.post(f"{base}/chat/completions", timeout=60,
                      headers={"Authorization": f"Bearer {os.environ.get('API_KEY', '')}"},
                      json={"model": os.environ.get("MODELO_LLM", "openai/gpt-6-luna"), "max_tokens": 500,
                            "reasoning": {"effort": "minimal"},
                            "response_format": {"type": "json_object"},
                            "messages": [{"role": "system", "content":
                                          "Você organiza o histórico de conversas do usuário com o assistente de voz Ollie. "
                                          "Responda só com JSON: {\"titulo\": até 6 palavras, sem ponto final, "
                                          "\"resumo\": 1 ou 2 frases em português do Brasil com o que foi pedido, "
                                          "respondido ou decidido, incluindo nomes, números e fatos úteis para lembrar depois}."},
                                         {"role": "user", "content": texto}]})
    r.raise_for_status()
    dados = json.loads(r.json()["choices"][0]["message"]["content"])
    return str(dados.get("titulo", "")).strip()[:80] or "Conversa", str(dados.get("resumo", "")).strip()[:600]


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
            alvo["titulo"], alvo["resumo"] = titulo, resumo or "Sem conteúdo relevante."
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
            texto = texto.replace(texto.splitlines()[0], f"# Conversas · {DIAS[dia.weekday()]}, {dia:%d/%m/%Y}", 1)
            (PASTA / destino).write_text(texto, encoding="utf-8")
            arq.unlink()
            itens.append({"id": inicio.strftime("%Y%m%d-%H%M%S"), "arquivo": destino,
                          "inicio": inicio.isoformat(timespec="seconds"), "fim": fim.isoformat(timespec="seconds"),
                          "falas": texto.count(" · Você:**") + texto.count(" · Ollie:**"), "titulo": "", "resumo": ""})
        _gravar_indice(itens)


def _loop() -> None:
    try:
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

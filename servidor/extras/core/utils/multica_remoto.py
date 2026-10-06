"""Controle do Multica deste Mac a partir do Watcher (CLI `multica`, já logado).

Tudo é síncrono (os handlers chamam via asyncio.to_thread) e nada é interativo:
- resumo: daemon local, runtimes, issues abertas e autopilots;
- issue: descrição, situação, agente e últimos comentários;
- ações: mudar a situação de uma issue, ligar/desligar o daemon e disparar um autopilot.

Criar issue e comentar ficam por voz (ferramentas multica_* da ponte MCP).
Todo subprocesso tem stdin=DEVNULL, prazo e grupo de processos próprio (morto no estouro do prazo).
"""

from __future__ import annotations

import json
import os
import re
import shutil
import signal
import subprocess
import time
from datetime import datetime, timezone
from pathlib import Path

try:
    from core.utils.idioma import IDIOMA, t as _t
except ImportError:  # carregado pela ponte MCP (fora do pacote core): lê o idioma.py ao lado
    import importlib.util as _ilu
    _spec = _ilu.spec_from_file_location("idioma", Path(__file__).resolve().with_name("idioma.py"))
    _idioma = _ilu.module_from_spec(_spec)
    _spec.loader.exec_module(_idioma)
    IDIOMA, _t = _idioma.IDIOMA, _idioma.t

LIMITE_ISSUES = 15
LIMITE_COMENTARIOS = 4
MAX_DESCRICAO = 600
MAX_COMENTARIO = 500
ONLINE_S = 180  # runtime visto há menos que isso conta como online
SEM_CLI = _t("A CLI do Multica não foi encontrada neste Mac.", "The Multica CLI was not found on this Mac.",
             "这台 Mac 上没有找到 Multica CLI。", "No se encontró la CLI de Multica en este Mac.")
# situações válidas do Multica, na ordem em que aparecem na tela
SITUACOES = {
    "in_progress": _t("Em andamento", "In progress", "进行中", "En curso"),
    "in_review": _t("Em revisão", "In review", "审核中", "En revisión"),
    "blocked": _t("Bloqueada", "Blocked", "已阻塞", "Bloqueada"),
    "todo": _t("A fazer", "To do", "待办", "Por hacer"),
    "backlog": "Backlog",
    "done": _t("Concluída", "Done", "已完成", "Completada"),
    "cancelled": _t("Cancelada", "Cancelled", "已取消", "Cancelada"),
}
FECHADAS = {"done", "cancelled"}
PRIORIDADES = {"urgent": _t("urgente", "urgent", "紧急", "urgente"), "high": _t("alta", "high", "高", "alta"),
               "medium": _t("média", "medium", "中", "media"), "low": _t("baixa", "low", "低", "baja")}


def _multica() -> str | None:
    achado = shutil.which("multica")
    if achado:
        return achado
    for c in (Path("/opt/homebrew/bin/multica"), Path("/usr/local/bin/multica"), Path.home() / ".local/bin/multica"):
        if c.exists():
            return str(c)
    return None


def _ambiente() -> dict:
    env = dict(os.environ)
    env.setdefault("HOME", str(Path.home()))
    env["NO_COLOR"] = "1"
    extras = [str(Path.home() / ".local/bin"), "/opt/homebrew/bin", "/usr/local/bin"]
    env["PATH"] = os.pathsep.join([*extras, env.get("PATH", "/usr/bin:/bin")])
    return env


def _rodar(args: list[str], prazo: float = 25) -> tuple[int, str, str]:
    """Roda `multica <args>` sem stdin; devolve (código, stdout, stderr). Código 127 = sem CLI, 124 = prazo."""
    exe = _multica()
    if not exe:
        return 127, "", SEM_CLI
    try:
        p = subprocess.Popen([exe, *args], stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                             stderr=subprocess.PIPE, text=True, env=_ambiente(), start_new_session=True)
    except OSError as e:
        return 127, "", _t("Não consegui rodar o Multica", "Couldn't run Multica", "无法运行 Multica", "No pude ejecutar Multica") + f": {e}"
    try:
        out, err = p.communicate(timeout=prazo)
        return p.returncode, out, err
    except subprocess.TimeoutExpired:
        try:
            os.killpg(p.pid, signal.SIGKILL)
        except OSError:
            pass
        return 124, "", _t("O Multica demorou demais para responder.", "Multica took too long to respond.",
                          "Multica 响应超时。", "Multica tardó demasiado en responder.")


def _json(args: list[str], prazo: float = 25):
    """Devolve (dados, erro). Acrescenta --output json."""
    rc, out, err = _rodar([*args, "--output", "json"], prazo)
    if rc != 0:
        return None, _corta(_limpar(err or out).removeprefix("Error:").strip() or f"erro {rc}", 200)
    try:
        return json.loads(out), None
    except ValueError:
        return None, _corta(_limpar(out), 200)


def _limpar(texto: str) -> str:
    texto = re.sub(r"\x1b\[[0-9;]*m", "", texto or "")
    return texto.replace(str(Path.home()), "~").strip()


def _corta(texto: str, n: int) -> str:
    texto = (texto or "").strip()
    return texto if len(texto) <= n else texto[: n - 1].rstrip() + "…"


def _markdown_simples(texto: str) -> str:
    """A tela não desenha Markdown: tira menções, links e marcações mais comuns."""
    texto = re.sub(r"\[([^\]]+)\]\((?:mention|https?)://[^)]*\)", r"\1", texto or "")
    texto = re.sub(r"^#{1,6}\s*", "", texto, flags=re.M)
    texto = re.sub(r"(\*\*|__|`)", "", texto)
    texto = re.sub(r"[\U0001F000-\U0001FAFF☀-➿️‍]+ ?", "", texto)  # emojis: a fonte da tela não tem
    texto = re.sub(r"\n{3,}", "\n\n", texto)
    return texto.strip()


def _quando(iso: str | None) -> str:
    try:
        d = datetime.fromisoformat(str(iso).replace("Z", "+00:00")).astimezone()
    except (ValueError, TypeError):
        return ""
    hoje = datetime.now().astimezone().date()
    if d.date() == hoje:
        return _t("hoje", "today", "今天", "hoy") + f" {d:%H:%M}"
    if (hoje - d.date()).days == 1:
        return _t("ontem", "yesterday", "昨天", "ayer") + f" {d:%H:%M}"
    return f"{d:%m/%d}" if IDIOMA in ("en-US", "zh-CN") else f"{d:%d/%m}"


# ---------------------------------------------------------------- nomes (agentes e membros), guardados por 5 min

_nomes = {"quando": 0.0, "dados": {}}


def _nomes_por_id() -> dict[str, str]:
    if _nomes["dados"] and time.time() - _nomes["quando"] < 300:
        return _nomes["dados"]
    nomes: dict[str, str] = {}
    agentes, _ = _json(["agent", "list"])
    for a in agentes if isinstance(agentes, list) else []:
        nomes[str(a.get("id"))] = str(a.get("name") or "")
    membros, _ = _json(["workspace", "members"])
    for m in membros if isinstance(membros, list) else []:
        nome = str(m.get("name") or m.get("email") or "")
        nomes[str(m.get("id"))] = nome
        nomes[str(m.get("user_id"))] = nome
    _nomes.update(quando=time.time(), dados=nomes)
    return nomes


def _nome(id_: str | None) -> str:
    return _nomes_por_id().get(str(id_), "") if id_ else ""


# ---------------------------------------------------------------- resumo

def status_daemon() -> dict:
    """{"ligado": bool, "detalhe": str}: daemon local e runtimes online da conta."""
    dados, erro = _json(["daemon", "status"], 15)
    st = str((dados or {}).get("status") or "") if isinstance(dados, dict) else ""
    ligado = st.lower() in ("running", "started", "online")
    runtimes, _ = _json(["runtime", "list"], 15)
    agora = datetime.now(timezone.utc)
    online = 0
    for r in runtimes if isinstance(runtimes, list) else []:
        try:
            visto = datetime.fromisoformat(str(r.get("last_seen_at")).replace("Z", "+00:00"))
            online += (agora - visto).total_seconds() < ONLINE_S
        except (ValueError, TypeError):
            pass
    detalhe = (_t("Ligado", "On", "已开启", "Activado") if ligado else _t("Desligado", "Off", "已关闭", "Desactivado"))
    if erro:
        detalhe = erro
    elif online:
        detalhe += " · " + _t(f"{online} runtime(s) online", f"{online} runtime(s) online", f"{online} 个运行时在线",
                              f"{online} runtime(s) en línea")
    return {"ligado": ligado, "detalhe": detalhe}


def _item_issue(i: dict) -> dict:
    st = str(i.get("status") or "")
    partes = [SITUACOES.get(st, st)]
    quem = _nome(i.get("assignee_id"))
    if quem:
        partes.append(quem)
    return {"id": str(i.get("identifier") or i.get("id")),
            "titulo": _corta(f"{i.get('identifier', '')} {i.get('title', '')}".strip(), 60),
            "detalhe": " · ".join(p for p in partes if p), "status": st}


def listar_issues(limite: int = LIMITE_ISSUES) -> tuple[list[dict], str | None]:
    """Issues abertas (não concluídas nem canceladas), da situação mais urgente para a menos."""
    dados, erro = _json(["issue", "list", "--limit", "100"])
    if erro:
        return [], erro
    issues = dados.get("issues", []) if isinstance(dados, dict) else dados or []
    abertas = [i for i in issues if str(i.get("status")) not in FECHADAS]
    ordem = list(SITUACOES)
    abertas.sort(key=lambda i: (ordem.index(i["status"]) if i.get("status") in ordem else 99,
                                -(i.get("number") or 0)))
    return [_item_issue(i) for i in abertas[:limite]], None


def listar_autopilots() -> list[dict]:
    dados, _ = _json(["autopilot", "list"])
    lista = dados.get("autopilots", []) if isinstance(dados, dict) else dados or []
    saida = []
    for a in lista if isinstance(lista, list) else []:
        titulo = a.get("title") or a.get("name") or a.get("id")
        quem = _nome(a.get("assignee_id") or a.get("agent_id"))
        ativo = str(a.get("status") or "active").lower() in ("active", "enabled", "")
        saida.append({"id": str(a.get("id")), "titulo": _corta(str(titulo), 50),
                      "detalhe": " · ".join(p for p in (quem, "" if ativo else _t("pausado", "paused", "已暂停", "pausado")) if p)})
    return saida


def resumo() -> dict:
    issues, erro = listar_issues()
    return {"daemon": status_daemon(), "issues": issues, "autopilots": [] if erro else listar_autopilots(), "erro": erro}


# ---------------------------------------------------------------- uma issue

def ler_issue(ident: str) -> dict | None:
    dados, erro = _json(["issue", "get", ident])
    if erro or not isinstance(dados, dict):
        return None
    st = str(dados.get("status") or "")
    cabecalho = [SITUACOES.get(st, st)]
    if _nome(dados.get("assignee_id")):
        cabecalho.append(_nome(dados.get("assignee_id")))
    if dados.get("priority") in PRIORIDADES:
        cabecalho.append(_t("prioridade ", "priority ", "优先级 ", "prioridad ") + PRIORIDADES[dados["priority"]])
    partes = [str(dados.get("title") or ""), " · ".join(cabecalho)]
    descricao = _markdown_simples(str(dados.get("description") or ""))
    if descricao:
        partes.append(_corta(descricao, MAX_DESCRICAO))
    comentarios, _ = _json(["issue", "comment", "list", ident])
    if isinstance(comentarios, list) and comentarios:
        for c in comentarios[-LIMITE_COMENTARIOS:]:
            autor = _nome(c.get("author_id")) or (_t("Agente", "Agent", "智能体", "Agente") if c.get("author_type") == "agent" else "")
            quando = _quando(c.get("created_at"))
            texto = _markdown_simples(str(c.get("content") or ""))
            # agentes escrevem o raciocínio antes da resposta: o fim é o que interessa
            if len(texto) > MAX_COMENTARIO:
                texto = "…" + texto[-(MAX_COMENTARIO - 1):].lstrip()
            partes.append(f"{autor}{' · ' + quando if quando else ''}\n{texto}")
    return {"id": str(dados.get("identifier") or ident), "titulo": str(dados.get("identifier") or ident),
            "status": st, "texto": "\n\n".join(p for p in partes if p)}


# ---------------------------------------------------------------- ações (o clique no aparelho é a confirmação)

def mudar_situacao(ident: str, situacao: str) -> dict:
    if situacao not in SITUACOES:
        return {"ok": False, "mensagem": _t("Situação inválida.", "Invalid status.", "状态无效。", "Estado no válido.")}
    rc, out, err = _rodar(["issue", "status", ident, situacao])
    if rc != 0:
        return {"ok": False, "mensagem": _corta(_limpar(err or out).removeprefix("Error:").strip(), 200)}
    return {"ok": True, "mensagem": f"{ident}: {SITUACOES[situacao]}."}


def definir_daemon(ligar: bool) -> dict:
    rc, out, err = _rodar(["daemon", "start" if ligar else "stop"], prazo=40)
    texto = _limpar(err or out).removeprefix("Error:").strip()
    if rc != 0:
        if "already" in texto.lower():
            return {"ok": True, "mensagem": _t("Já estava assim.", "It was already like that.", "本来就是这样。", "Ya estaba así.")}
        return {"ok": False, "mensagem": _corta(texto or _t("Falhou.", "Failed.", "失败。", "Falló."), 200)}
    return {"ok": True, "mensagem": _t("Daemon ligado: os agentes deste Mac pegam as tarefas.",
                                       "Daemon on: this Mac's agents will pick up tasks.",
                                       "守护进程已开启：这台 Mac 上的智能体会领取任务。",
                                       "Daemon activado: los agentes de este Mac tomarán las tareas.") if ligar else
            _t("Daemon desligado.", "Daemon off.", "守护进程已关闭。", "Daemon desactivado.")}


def disparar_autopilot(id_: str) -> dict:
    rc, out, err = _rodar(["autopilot", "trigger", id_], prazo=30)
    if rc != 0:
        return {"ok": False, "mensagem": _corta(_limpar(err or out).removeprefix("Error:").strip(), 200)}
    return {"ok": True, "mensagem": _t("Autopilot disparado.", "Autopilot triggered.", "Autopilot 已触发。", "Autopilot lanzado.")}

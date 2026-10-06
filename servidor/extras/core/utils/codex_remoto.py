"""Controle do Codex (OpenAI Codex CLI) deste Mac a partir do Watcher.

Tudo é síncrono (os handlers chamam via asyncio.to_thread) e nada é interativo:
- sessões locais: lidas do banco do Codex (~/.codex/state_*.sqlite, só leitura) e dos arquivos
  rollout JSONL em ~/.codex/sessions/AAAA/MM/DD/;
- mandar mensagem: `codex queue` (fica na fila da sessão) ou `codex exec resume` em segundo plano
  (o Codex roda a vez e consome também o que estava na fila);
- tarefa nova: `codex exec` em segundo plano (somente leitura, a não ser que o app confirme escrita);
- controle remoto: `codex remote-control [start|stop] --json`;
- Codex Cloud: `codex cloud list --json` e `codex cloud status|diff`.

Todo subprocesso tem stdin=DEVNULL, prazo e grupo de processos próprio (morto no estouro do prazo).
"""

from __future__ import annotations

import json
import os
import re
import shutil
import signal
import sqlite3
import subprocess
import threading
import time
from datetime import datetime
from pathlib import Path

try:
    from core.utils.idioma import IDIOMA, t as _t
except ImportError:  # carregado pela ponte MCP (fora do pacote core): lê o idioma.py ao lado
    import importlib.util as _ilu
    _spec = _ilu.spec_from_file_location("idioma", Path(__file__).resolve().with_name("idioma.py"))
    _idioma = _ilu.module_from_spec(_spec)
    _spec.loader.exec_module(_idioma)
    IDIOMA, _t = _idioma.IDIOMA, _idioma.t

CODEX_HOME = Path(os.environ.get("CODEX_HOME") or Path.home() / ".codex")
LIMITE_SESSOES = 12
LIMITE_MSGS = 12
MAX_MSG = 500
MAX_TOTAL = 5000
MAX_ENVIO = 4000
PRAZO_TAREFA_S = 30 * 60  # tarefa em segundo plano é encerrada depois disso
DIAS = _t("Seg Ter Qua Qui Sex Sáb Dom", "Mon Tue Wed Thu Fri Sat Sun", "周一 周二 周三 周四 周五 周六 周日",
          "Lun Mar Mié Jue Vie Sáb Dom").split()
VOCE = _t("Você", "You", "你", "Tú")
SEM_CLI = _t("A CLI do Codex não foi encontrada neste Mac.", "The Codex CLI was not found on this Mac.",
             "这台 Mac 上没有找到 Codex CLI。", "No se encontró la CLI de Codex en este Mac.")
NUVEM_NAO_ENCONTRADA = _t("Tarefa não encontrada no Codex Cloud.", "Task not found in Codex Cloud.",
                          "Codex Cloud 中没有找到该任务。", "Tarea no encontrada en Codex Cloud.")
# _estado devolve estes valores em português (comparados no código); a tela recebe o rótulo do idioma
ROTULOS_ESTADO = {
    "Trabalhando": _t("Trabalhando", "Working", "工作中", "Trabajando"),
    "Concluída": _t("Concluída", "Done", "已完成", "Completada"),
    "Limite de uso": _t("Limite de uso", "Usage limit", "达到用量上限", "Límite de uso"),
    "Erro": _t("Erro", "Error", "错误", "Error"),
    "Interrompida": _t("Interrompida", "Interrupted", "已中断", "Interrumpida"),
    "Parada": _t("Parada", "Idle", "已停止", "Detenida"),
}
NA_FILA = _t("[na fila] ", "[queued] ", "[排队中] ", "[en cola] ")
SOMENTE_LEITURA = _t(" (somente leitura).", " (read-only).", "（只读）。", " (solo lectura).")
FILA_AO_TERMINAR = _t("Na fila: o Codex lê ao terminar o que está fazendo.",
                     "Queued: Codex reads it when it finishes what it is doing.",
                     "已排队：Codex 完成当前工作后会读取。",
                     "En cola: Codex lo lee al terminar lo que está haciendo.")
FILA_PROXIMA_VEZ = _t("Na fila: o Codex lê quando a sessão rodar de novo.",
                      "Queued: Codex reads it when the session runs again.",
                      "已排队：会话再次运行时 Codex 会读取。",
                      "En cola: Codex lo lee cuando la sesión vuelva a ejecutarse.")
LIGADO = _t("Ligado", "On", "已开启", "Activado")
DESLIGADO = _t("Desligado", "Off", "已关闭", "Desactivado")
TAREFA = _t("Tarefa", "Task", "任务", "Tarea")
PODE_ALTERAR = _t(" (pode alterar arquivos).", " (may change files).", "（可以修改文件）。", " (puede modificar archivos).")

# tarefas em segundo plano lançadas por este servidor: id da sessão -> dados
_tarefas: dict[str, dict] = {}
_trava = threading.Lock()


# ---------------------------------------------------------------- utilidades

def _codex() -> str | None:
    achado = shutil.which("codex")
    if achado:
        return achado
    for c in (Path.home() / ".local/bin/codex", Path("/opt/homebrew/bin/codex"), Path("/usr/local/bin/codex")):
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


def _rodar(args: list[str], prazo: float = 25, cwd: str | None = None) -> tuple[int, str, str]:
    """Roda `codex <args>` sem stdin; devolve (código, stdout, stderr). Código 127 = sem codex, 124 = prazo."""
    exe = _codex()
    if not exe:
        return 127, "", SEM_CLI
    try:
        p = subprocess.Popen([exe, *args], stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                             stderr=subprocess.PIPE, text=True, cwd=cwd, env=_ambiente(),
                             start_new_session=True)
    except OSError as e:
        return 127, "", _t("Não consegui rodar o Codex", "Couldn't run Codex", "无法运行 Codex", "No pude ejecutar Codex") + f": {e}"
    try:
        out, err = p.communicate(timeout=prazo)
        return p.returncode, out, err
    except subprocess.TimeoutExpired:
        _matar(p)
        return 124, "", _t("O Codex demorou demais para responder.", "Codex took too long to respond.",
                          "Codex 响应超时。", "Codex tardó demasiado en responder.")


def _matar(p: subprocess.Popen) -> None:
    try:
        os.killpg(p.pid, signal.SIGTERM)
        p.wait(timeout=5)
    except (OSError, subprocess.TimeoutExpired):
        try:
            os.killpg(p.pid, signal.SIGKILL)
        except OSError:
            pass
    try:
        p.communicate(timeout=2)
    except (OSError, ValueError, subprocess.TimeoutExpired):
        pass


def _limpar(texto: str) -> str:
    """Tira cores ANSI, nome do computador e a pasta pessoal do texto que vai para a tela."""
    texto = re.sub(r"\x1b\[[0-9;]*m", "", texto or "")
    texto = re.sub(r"\bon [\w.-]+\.local\b", _t("neste Mac", "on this Mac", "在这台 Mac 上", "en este Mac"), texto)
    return texto.replace(str(Path.home()), "~").strip()


def _corta(texto: str, n: int) -> str:
    texto = (texto or "").strip()
    return texto if len(texto) <= n else texto[: n - 1].rstrip() + "…"


def _quando(ts: float | None) -> str:
    if not ts:
        return ""
    d = datetime.fromtimestamp(ts)
    hoje = datetime.now().date()
    if d.date() == hoje:
        return _t("hoje", "today", "今天", "hoy") + f" {d:%H:%M}"
    if (hoje - d.date()).days == 1:
        return _t("ontem", "yesterday", "昨天", "ayer") + f" {d:%H:%M}"
    return f"{DIAS[d.weekday()]} {d:%m/%d}" if IDIOMA in ("en-US", "zh-CN") else f"{DIAS[d.weekday()]} {d:%d/%m}"


def _hora_iso(iso: str) -> str:
    try:
        return datetime.fromisoformat(iso.replace("Z", "+00:00")).astimezone().strftime("%H:%M")
    except (ValueError, AttributeError):
        return ""


def _pasta_curta(cwd: str) -> str:
    return Path(cwd).name if cwd else ""


# ---------------------------------------------------------------- banco local do Codex

def _banco(prefixo: str) -> Path | None:
    candidatos = sorted(CODEX_HOME.glob(f"{prefixo}_*.sqlite"),
                        key=lambda p: int(re.sub(r"\D", "", p.stem) or 0), reverse=True)
    return candidatos[0] if candidatos else None


def _consulta(prefixo: str, sql: str, params: tuple = ()) -> list[sqlite3.Row]:
    arq = _banco(prefixo)
    if not arq:
        return []
    try:
        con = sqlite3.connect(f"file:{arq}?mode=ro", uri=True, timeout=3)
        con.row_factory = sqlite3.Row
        try:
            return con.execute(sql, params).fetchall()
        finally:
            con.close()
    except sqlite3.Error:
        return []


def _nomes_indice() -> dict[str, str]:
    """Nomes dados às sessões (session_index.jsonl): id -> nome."""
    nomes = {}
    try:
        for linha in (CODEX_HOME / "session_index.jsonl").read_text(encoding="utf-8").splitlines():
            try:
                o = json.loads(linha)
                if o.get("id") and o.get("thread_name"):
                    nomes[o["id"]] = o["thread_name"]
            except ValueError:
                continue
    except OSError:
        pass
    return nomes


def _na_fila(thread_id: str | None = None) -> dict[str, list[dict]]:
    """Mensagens enfileiradas (`codex queue`) por sessão."""
    sql = "select thread_id, payload_json, created_at_ms from queued_items"
    linhas = _consulta("queue", sql + (" where thread_id = ?" if thread_id else "") + " order by queue_order",
                       (thread_id,) if thread_id else ())
    fila: dict[str, list[dict]] = {}
    for r in linhas:
        try:
            conteudo = json.loads(r["payload_json"]).get("UserInput", {}).get("content", [])
            texto = " ".join(c.get("text", "") for c in conteudo if c.get("type") == "text")
        except (ValueError, AttributeError):
            texto = ""
        fila.setdefault(r["thread_id"], []).append({"texto": texto, "ms": r["created_at_ms"]})
    return fila


def _sessao(thread_id: str) -> dict | None:
    linhas = _consulta("state", "select id, rollout_path, cwd, title, name, source, recency_at_ms, updated_at "
                                "from threads where id = ?", (thread_id,))
    if linhas:
        r = dict(linhas[0])
        r["titulo"] = r.get("name") or _nomes_indice().get(thread_id) or r.get("title") or _t("Sessão", "Session", "会话", "Sesión")
        return r
    # sem banco: procura o arquivo rollout pelo id
    if not re.fullmatch(r"[0-9a-fA-F-]{8,64}", thread_id or ""):
        return None
    achados = sorted((CODEX_HOME / "sessions").glob(f"*/*/*/rollout-*{thread_id}.jsonl"))
    if not achados:
        return None
    meta = _meta(achados[-1])
    return {"id": thread_id, "rollout_path": str(achados[-1]), "cwd": meta.get("cwd", ""),
            "titulo": _nomes_indice().get(thread_id) or _t("Sessão", "Session", "会话", "Sesión"), "source": meta.get("source", ""),
            "recency_at_ms": int(achados[-1].stat().st_mtime * 1000)}


def _meta(arq: Path) -> dict:
    try:
        with arq.open(encoding="utf-8") as f:
            o = json.loads(f.readline())
        return o.get("payload", {}) if o.get("type") == "session_meta" else {}
    except (OSError, ValueError):
        return {}


def _linhas_finais(arq: Path, max_bytes: int) -> list[str]:
    try:
        with arq.open("rb") as f:
            f.seek(0, os.SEEK_END)
            tam = f.tell()
            f.seek(max(0, tam - max_bytes))
            bruto = f.read()
    except OSError:
        return []
    linhas = bruto.decode("utf-8", "replace").splitlines()
    return linhas[1:] if tam > max_bytes else linhas  # a primeira pode estar cortada


def _estado(arq: Path, thread_id: str) -> str:
    """Trabalhando | Concluída | Limite de uso | Erro | Interrompida | Parada, pelo fim do rollout."""
    with _trava:
        t = _tarefas.get(thread_id)
        if t and t["proc"].poll() is None:
            return "Trabalhando"
    try:
        idade = time.time() - arq.stat().st_mtime
    except OSError:
        return "Parada"
    for linha in reversed(_linhas_finais(arq, 256 * 1024)):
        if '"event_msg"' not in linha:
            continue
        try:
            p = json.loads(linha).get("payload", {})
        except ValueError:
            continue
        tipo = p.get("type")
        if tipo == "task_started":
            return "Trabalhando" if idade < 20 * 60 else "Interrompida"
        if tipo == "turn_aborted":
            return "Interrompida"
        if tipo == "task_complete":
            erro = (p.get("error") or {}).get("message", "") if isinstance(p.get("error"), dict) else ""
            if erro:
                return "Limite de uso" if "usage limit" in erro.lower() else "Erro"
            return "Concluída"
    return "Parada"


# ---------------------------------------------------------------- a) listar sessões

def listar_sessoes(limite: int = LIMITE_SESSOES) -> list[dict]:
    """Sessões locais mais recentes (sem subagentes nem execuções triviais): [{id, titulo, detalhe}]."""
    nomes = _nomes_indice()
    linhas = _consulta(
        "state",
        "select id, rollout_path, cwd, title, name, source, recency_at_ms from threads "
        "where archived = 0 and preview <> '' and source not like '%subagent%' "
        "order by recency_at_ms desc limit 400")
    candidatos = []
    for r in linhas:
        titulo = (r["name"] or nomes.get(r["id"]) or r["title"] or "").strip()
        primeira = titulo.splitlines()[0].strip() if titulo else ""
        # descarta ruído: sessões automáticas de "status", testes curtos etc.
        if not (r["name"] or r["id"] in nomes) and (len(primeira) < 15 or primeira.startswith(("/", "$"))):
            continue
        # `codex exec` de scripts vira ruído: só entra se foi lançado por aqui ou é das últimas 24 h
        if r["source"] == "exec" and r["id"] not in _tarefas and (r["recency_at_ms"] or 0) < (time.time() - 86400) * 1000:
            continue
        candidatos.append((r["id"], primeira, r["rollout_path"], r["cwd"], (r["recency_at_ms"] or 0) / 1000))
        if len(candidatos) >= limite:
            break
    if not linhas:  # sem banco: arquivos rollout por data de modificação
        arqs = sorted((CODEX_HOME / "sessions").glob("*/*/*/rollout-*.jsonl"),
                      key=lambda p: p.stat().st_mtime, reverse=True)[:limite]
        for a in arqs:
            m = re.search(r"([0-9a-f]{8}-[0-9a-f-]{27})\.jsonl$", a.name)
            if m:
                meta = _meta(a)
                candidatos.append((m.group(1), nomes.get(m.group(1)) or _t("Sessão", "Session", "会话", "Sesión"), str(a), meta.get("cwd", ""),
                                   a.stat().st_mtime))
    fila = _na_fila()
    sessoes = []
    for tid, titulo, rollout, cwd, ts in candidatos:
        partes = [ROTULOS_ESTADO[_estado(Path(rollout), tid)], _pasta_curta(cwd), _quando(ts)]
        if fila.get(tid):
            partes.append(f"{len(fila[tid])} " + _t("na fila", "queued", "条排队中", "en cola"))
        sessoes.append({"id": tid, "titulo": _corta(titulo, 40), "detalhe": " · ".join(p for p in partes if p)})
    return sessoes


# ---------------------------------------------------------------- b) ler mensagens

def _texto_usuario(conteudo: list) -> str:
    partes = []
    for c in conteudo or []:
        t = c.get("text", "") if c.get("type") in ("input_text", "text") else ""
        t = t.strip()
        if not t or t.startswith("<") or t.startswith("# AGENTS.md"):
            continue  # contexto injetado pelo Codex, não é fala do usuário
        partes.append(t)
    return "\n".join(partes)


def ler_sessao(thread_id: str) -> dict | None:
    """{titulo, situacao, mensagens: [{quem, hora, texto}]} — últimas mensagens, da mais antiga à mais nova."""
    s = _sessao(thread_id)
    if not s:
        return None
    arq = Path(s["rollout_path"])
    msgs = []
    for linha in _linhas_finais(arq, 4 * 1024 * 1024):
        if '"message"' not in linha:
            continue
        try:
            o = json.loads(linha)
        except ValueError:
            continue
        p = o.get("payload", {})
        if o.get("type") != "response_item" or p.get("type") != "message":
            continue
        if p.get("role") == "user":
            texto, quem = _texto_usuario(p.get("content")), VOCE
        elif p.get("role") == "assistant":
            texto = "\n".join(c.get("text", "") for c in p.get("content") or [] if c.get("type") == "output_text")
            quem = "Codex"
        else:
            continue
        if texto.strip():
            msgs.append({"quem": quem, "hora": _hora_iso(o.get("timestamp", "")), "texto": _corta(texto, MAX_MSG)})
    for item in _na_fila(thread_id).get(thread_id, []):
        msgs.append({"quem": VOCE, "hora": datetime.fromtimestamp(item["ms"] / 1000).strftime("%H:%M"),
                     "texto": _corta(NA_FILA + item["texto"], MAX_MSG)})
    with _trava:
        t = _tarefas.get(thread_id)
        for texto in (t or {}).get("pendentes", []):
            msgs.append({"quem": VOCE, "hora": "", "texto": _corta(NA_FILA + texto, MAX_MSG)})
    msgs = msgs[-LIMITE_MSGS:]
    while len(msgs) > 1 and sum(len(m["texto"]) for m in msgs) > MAX_TOTAL:
        msgs.pop(0)
    with _trava:
        t = _tarefas.get(thread_id)
        if t and t["proc"].poll() is not None and t.get("erro"):
            msgs.append({"quem": "Codex", "hora": "", "texto": _corta(_t("Erro: ", "Error: ", "错误：", "Error: ") + t["erro"], MAX_MSG)})
    return {"titulo": _corta(s["titulo"].splitlines()[0], 40), "situacao": ROTULOS_ESTADO[_estado(arq, thread_id)], "mensagens": msgs}


# ---------------------------------------------------------------- execução em segundo plano

def _lancar(args: list[str], cwd: str, sandbox: str, chave: str | None = None, prazo_id: float = 0) -> dict:
    """Roda `codex exec ... --json` em segundo plano. Com prazo_id, espera o thread_id aparecer.
    O que for enviado enquanto roda fica em "pendentes" e vira um `exec resume` quando ele terminar
    (um `codex queue` no meio da vez seria descartado: o exec encerra e aborta a vez seguinte)."""
    exe = _codex()
    if not exe:
        return {"ok": False, "mensagem": SEM_CLI}
    try:
        p = subprocess.Popen([exe, *args], stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                             stderr=subprocess.DEVNULL, text=True, cwd=cwd, env=_ambiente(),
                             start_new_session=True)
    except OSError as e:
        return {"ok": False, "mensagem": _t("Não consegui iniciar o Codex", "Couldn't start Codex", "无法启动 Codex", "No pude iniciar Codex") + f": {e}"}
    tarefa = {"proc": p, "inicio": time.time(), "thread_id": chave, "erro": "", "ultima": "",
              "pendentes": [], "sandbox": sandbox, "cwd": cwd}
    pronto = threading.Event()

    def ler():
        for linha in p.stdout:  # drena a saída para o processo nunca travar no pipe
            try:
                o = json.loads(linha)
            except ValueError:
                continue
            tipo = o.get("type")
            if tipo == "thread.started":
                tarefa["thread_id"] = o.get("thread_id")
                with _trava:
                    _tarefas[tarefa["thread_id"]] = tarefa
                pronto.set()
            elif tipo == "item.completed" and (o.get("item") or {}).get("type") == "agent_message":
                tarefa["ultima"] = o["item"].get("text", "")
            elif tipo in ("turn.failed", "error"):
                tarefa["erro"] = _limpar(str((o.get("error") or {}).get("message") or o.get("message") or ""))
        pronto.set()

    def vigiar():
        try:
            p.wait(timeout=PRAZO_TAREFA_S)
        except subprocess.TimeoutExpired:
            tarefa["erro"] = _t("Tempo esgotado: a tarefa foi encerrada pelo servidor.",
                                "Timed out: the server stopped the task.", "超时：服务器已结束该任务。",
                                "Tiempo agotado: el servidor detuvo la tarea.")
            _matar(p)
        with _trava:
            pendentes, tarefa["pendentes"] = tarefa["pendentes"], []
        if pendentes and tarefa["thread_id"]:
            _retomar(tarefa["thread_id"], "\n\n".join(pendentes), sandbox, cwd)

    threading.Thread(target=ler, daemon=True).start()
    threading.Thread(target=vigiar, daemon=True).start()
    if chave:
        with _trava:
            _tarefas[chave] = tarefa
    if prazo_id:
        pronto.wait(prazo_id)
        if p.poll() is not None and not tarefa["thread_id"]:
            return {"ok": False, "mensagem": tarefa["erro"] or _t("O Codex encerrou sem criar a sessão.",
                                                                  "Codex exited without creating the session.",
                                                                  "Codex 未创建会话就退出了。",
                                                                  "Codex terminó sin crear la sesión.")}
    return {"ok": True, "id": tarefa["thread_id"]}


def _retomar(thread_id: str, texto: str, sandbox: str, cwd: str) -> dict:
    return _lancar(["exec", "resume", "--skip-git-repo-check", "-c", f'sandbox_mode="{sandbox}"', "--json",
                    thread_id, texto], cwd=cwd, sandbox=sandbox, chave=thread_id)


def _sandbox(escrita: bool, confirmado: bool) -> tuple[str | None, str]:
    if not escrita:
        return "read-only", ""
    if not confirmado:
        return None, _t("Alterar arquivos precisa de confirmação: reenvie com \"confirmado\": true.",
                        "Changing files needs confirmation: resend with \"confirmado\": true.",
                        "修改文件需要确认：请带上 \"confirmado\": true 重新发送。",
                        "Modificar archivos necesita confirmación: reenvía con \"confirmado\": true.")
    return "workspace-write", ""


# ---------------------------------------------------------------- c) mandar mensagem

def enviar(thread_id: str, texto: str, modo: str = "auto", escrita: bool = False,
           confirmado: bool = False) -> dict:
    """modo "fila": `codex queue` (o Codex lê quando a sessão rodar de novo, no app ou no terminal);
    "executar": `codex exec resume` em segundo plano (somente leitura, salvo escrita confirmada);
    "auto": fila se a sessão está trabalhando, senão executar."""
    texto = (texto or "").strip()
    if not texto:
        return {"ok": False, "mensagem": _t("Mensagem vazia.", "Empty message.", "消息为空。", "Mensaje vacío.")}
    if len(texto) > MAX_ENVIO:
        return {"ok": False, "mensagem": _t(f"Mensagem longa demais (máx. {MAX_ENVIO} caracteres).",
                                                f"Message too long (max. {MAX_ENVIO} characters).",
                                                f"消息过长（最多 {MAX_ENVIO} 个字符）。",
                                                f"Mensaje demasiado largo (máx. {MAX_ENVIO} caracteres).")}
    s = _sessao(thread_id)
    if not s:
        return {"ok": False, "mensagem": _t("Sessão não encontrada.", "Session not found.", "没有找到会话。",
                                             "Sesión no encontrada.")}
    with _trava:  # tarefa lançada por aqui ainda rodando: guarda e envia quando ela terminar
        t = _tarefas.get(thread_id)
        if t and t["proc"].poll() is None:
            t["pendentes"].append(texto)
            return {"ok": True, "modo": "fila", "mensagem": FILA_AO_TERMINAR}
    estado = _estado(Path(s["rollout_path"]), thread_id)
    if modo not in ("fila", "executar"):
        modo = "fila" if estado == "Trabalhando" else "executar"
    if modo == "fila":
        rc, out, err = _rodar(["queue", "--thread", thread_id, "--message", texto], prazo=20)
        if rc != 0:
            return {"ok": False, "mensagem": _corta(_limpar(err or out) or _t("Falha ao enfileirar.", "Failed to queue.", "排队失败。", "Error al poner en cola."), 300)}
        return {"ok": True, "modo": "fila",
                "mensagem": FILA_PROXIMA_VEZ if estado != "Trabalhando"
                else FILA_AO_TERMINAR}
    if estado == "Trabalhando":
        return {"ok": False, "mensagem": _t("A sessão está trabalhando agora; use a fila.", "The session is working now; use the queue.",
                                            "会话正在工作；请使用排队。", "La sesión está trabajando ahora; usa la cola.")}
    sandbox, erro = _sandbox(escrita, confirmado)
    if not sandbox:
        return {"ok": False, "precisa_confirmar": True, "mensagem": erro}
    cwd = s.get("cwd") if s.get("cwd") and Path(s["cwd"]).is_dir() else str(Path.home())
    r = _retomar(thread_id, texto, sandbox, cwd)
    if not r["ok"]:
        return r
    return {"ok": True, "modo": "executar",
            "mensagem": _t("Enviado: o Codex está trabalhando", "Sent: Codex is working", "已发送：Codex 正在工作",
                           "Enviado: Codex está trabajando") + (PODE_ALTERAR if sandbox != "read-only" else SOMENTE_LEITURA)}


# ---------------------------------------------------------------- d) tarefa nova

def iniciar_tarefa(texto: str, pasta: str | None = None, escrita: bool = False, confirmado: bool = False) -> dict:
    """Nova sessão `codex exec` em segundo plano. Devolve {ok, id, mensagem}."""
    texto = (texto or "").strip()
    if not texto:
        return {"ok": False, "mensagem": _t("Tarefa vazia.", "Empty task.", "任务为空。", "Tarea vacía.")}
    if len(texto) > MAX_ENVIO:
        return {"ok": False, "mensagem": _t(f"Tarefa longa demais (máx. {MAX_ENVIO} caracteres).",
                                                f"Task too long (max. {MAX_ENVIO} characters).",
                                                f"任务过长（最多 {MAX_ENVIO} 个字符）。",
                                                f"Tarea demasiado larga (máx. {MAX_ENVIO} caracteres).")}
    pasta = os.path.expanduser(pasta or os.environ.get("WATCHER_CODEX_PASTA") or "~")
    if not Path(pasta).is_dir():
        return {"ok": False, "mensagem": _t("Pasta não encontrada.", "Folder not found.", "没有找到文件夹。",
                                             "Carpeta no encontrada.")}
    sandbox, erro = _sandbox(escrita, confirmado)
    if not sandbox:
        return {"ok": False, "precisa_confirmar": True, "mensagem": erro}
    r = _lancar(["exec", "--skip-git-repo-check", "-s", sandbox, "-C", pasta, "--json", texto],
                cwd=pasta, sandbox=sandbox, prazo_id=30)
    if not r["ok"]:
        return r
    return {"ok": True, "id": r["id"], "mensagem": _t("Tarefa iniciada", "Task started", "任务已开始", "Tarea iniciada")
            + (SOMENTE_LEITURA if sandbox == "read-only" else PODE_ALTERAR)}


# ---------------------------------------------------------------- e) controle remoto

def _daemon_vivo() -> bool:
    try:
        pid = json.loads((CODEX_HOME / "app-server-daemon/app-server.pid").read_text())["pid"]
        os.kill(int(pid), 0)
        return True
    except (OSError, ValueError, KeyError, TypeError):
        return False


def status_remoto() -> dict:
    """{"ligado": bool, "detalhe": str} a partir de `codex remote-control --json`."""
    rc, out, err = _rodar(["remote-control", "--json"], prazo=20)
    texto = _limpar(out.strip() or err.strip())
    dados = None
    try:
        dados = json.loads(out) if out.strip().startswith("{") else None
    except ValueError:
        pass
    if rc == 0 and isinstance(dados, dict):
        st = str(dados.get("status") or dados.get("state") or "")
        ligado = st.lower() not in ("", "notrunning", "stopped", "disabled", "off")
        return {"ligado": ligado, "detalhe": {"running": LIGADO, "connected": _t("Ligado e conectado", "On and connected",
                                                                                "已开启并已连接", "Activado y conectado"),
                                              "notRunning": DESLIGADO}.get(st, st or _t("Desconhecido", "Unknown",
                                                                                         "未知", "Desconocido"))}
    if rc == 124 or rc == 127:
        return {"ligado": False, "detalhe": texto}
    msg = texto.removeprefix("Error:").strip()
    if "errored" in msg:
        return {"ligado": False, "detalhe": _t("Habilitado na conta, mas sem conexão", "Enabled on the account, but not connected",
                                               "账户已启用，但未连接", "Habilitado en la cuenta, pero sin conexión")
                + ("" if _daemon_vivo() else _t(" (serviço parado)", " (service stopped)", "（服务已停止）", " (servicio detenido)")) + "."}
    return {"ligado": rc == 0, "detalhe": _corta(msg, 120) or (LIGADO if rc == 0 else DESLIGADO)}


def definir_remoto(ligar: bool, confirmado: bool = False) -> dict:
    """Liga (`remote-control start`) ou desliga (`remote-control stop`) o serviço de controle remoto.
    Ligar expõe este Mac ao Codex da sua conta em outros aparelhos: exige confirmação."""
    if ligar and not confirmado:
        return {"ok": False, "precisa_confirmar": True,
                "mensagem": _t("Ligar deixa o Codex desta conta controlar o Mac de outros aparelhos. "
                               "Confirme no app para continuar.",
                               "Turning it on lets this account's Codex control the Mac from other devices. "
                               "Confirm in the app to continue.",
                               "开启后，此账户的 Codex 可以从其他设备控制这台 Mac。请在应用中确认以继续。",
                               "Activarlo permite que el Codex de esta cuenta controle el Mac desde otros dispositivos. "
                               "Confirma en la app para continuar.")}
    rc, out, err = _rodar(["remote-control", "start" if ligar else "stop", "--json"], prazo=60)
    texto = _limpar(out or err)
    try:
        dados = json.loads(out)
    except ValueError:
        dados = None
    if rc != 0:
        if "standalone" in texto:
            texto = _t("Precisa da instalação oficial do Codex (instalador da OpenAI) para o serviço remoto.",
                       "The remote service needs the official Codex install (OpenAI installer).",
                       "远程服务需要官方安装的 Codex（OpenAI 安装程序）。",
                       "El servicio remoto necesita la instalación oficial de Codex (instalador de OpenAI).")
        return {"ok": False, "mensagem": _corta(texto or _t("Falhou.", "Failed.", "失败。", "Falló."), 300)}
    st = str((dados or {}).get("status", ""))
    if ligar:
        return {"ok": True, "mensagem": _t("Controle remoto ligado.", "Remote control on.", "远程控制已开启。",
                                           "Control remoto activado.") + (f" ({st})" if st else "")}
    return {"ok": True, "mensagem": _t("Já estava desligado.", "It was already off.", "本来就是关闭的。", "Ya estaba desactivado.")
            if st == "notRunning" else _t("Controle remoto desligado.", "Remote control off.", "远程控制已关闭。",
                                          "Control remoto desactivado.")}


# ---------------------------------------------------------------- f) Codex Cloud

def _erro_nuvem(texto: str) -> str:
    texto = _limpar(texto)
    if "Not signed in" in texto or "codex login" in texto:
        return _t("Codex Cloud: entre na conta no Mac (codex login).", "Codex Cloud: sign in on the Mac (codex login).",
                  "Codex Cloud：请在 Mac 上登录（codex login）。", "Codex Cloud: inicia sesión en el Mac (codex login).")
    if "404" in texto:
        return NUVEM_NAO_ENCONTRADA
    return _corta(texto.removeprefix("Error:").strip() or _t("Codex Cloud indisponível.", "Codex Cloud unavailable.",
                                                              "Codex Cloud 不可用。", "Codex Cloud no disponible."), 200)


def listar_nuvem(limite: int = 10) -> tuple[list[dict], str | None]:
    """([{id, titulo, detalhe}], erro)."""
    rc, out, err = _rodar(["cloud", "list", "--json", "--limit", str(max(1, min(20, limite)))], prazo=30)
    if rc != 0:
        return [], _erro_nuvem(err or out)
    try:
        tarefas = json.loads(out).get("tasks") or []
    except (ValueError, AttributeError):
        return [], _t("Resposta inesperada do Codex Cloud.", "Unexpected response from Codex Cloud.",
                      "Codex Cloud 返回了意外的响应。", "Respuesta inesperada de Codex Cloud.")
    itens = []
    for t in tarefas:
        if not isinstance(t, dict) or not t.get("id"):
            continue
        partes = [str(t.get("status") or "").replace("_", " ").capitalize()]
        env = t.get("environment_label") or t.get("environment") or t.get("env") or ""
        if isinstance(env, dict):
            env = env.get("label") or env.get("name") or ""
        partes.append(str(env))
        quando = t.get("updated_at") or t.get("created_at")
        if isinstance(quando, (int, float)):
            partes.append(_quando(quando / 1000 if quando > 1e11 else quando))
        elif isinstance(quando, str):
            try:
                partes.append(_quando(datetime.fromisoformat(quando.replace("Z", "+00:00")).timestamp()))
            except ValueError:
                pass
        resumo = t.get("summary") or t.get("diff_stats") or {}
        if isinstance(resumo, dict) and resumo.get("files_changed"):
            partes.append(f"{resumo['files_changed']} " + _t("arquivos", "files", "个文件", "archivos"))
        itens.append({"id": str(t["id"]), "titulo": _corta(str(t.get("title") or TAREFA), 40),
                      "detalhe": " · ".join(p for p in partes if p)})
    return itens, None


def ler_nuvem(task_id: str) -> dict:
    """{titulo, texto}: status da tarefa + arquivos do diff. {"erro"} se falhar."""
    if not re.fullmatch(r"[\w-]{4,100}", task_id or ""):
        return {"erro": _t("Id de tarefa inválido.", "Invalid task id.", "任务 ID 无效。", "Id de tarea no válido.")}
    rc, out, err = _rodar(["cloud", "status", task_id], prazo=30)
    if rc != 0:
        return {"erro": _erro_nuvem(err or out)}
    status = _limpar(out)
    linhas = [l for l in status.splitlines() if l.strip()]
    titulo = linhas[0] if linhas else TAREFA
    texto = status
    rc, diff, _ = _rodar(["cloud", "diff", task_id], prazo=30)
    if rc == 0 and diff.strip():
        arquivos = re.findall(r"^\+\+\+ b/(.+)$", diff, flags=re.M)
        mais = sum(1 for l in diff.splitlines() if l.startswith("+") and not l.startswith("+++"))
        menos = sum(1 for l in diff.splitlines() if l.startswith("-") and not l.startswith("---"))
        if arquivos:
            texto += ("\n\n" + _t("Alterações", "Changes", "修改", "Cambios") + f": {len(arquivos)} "
                      + _t("arquivos", "files", "个文件", "archivos") + f", +{mais} −{menos}\n") + "\n".join(
                "• " + a for a in arquivos[:15]) + ("\n…" if len(arquivos) > 15 else "")
    return {"titulo": _corta(titulo, 40), "texto": _corta(texto, MAX_TOTAL)}

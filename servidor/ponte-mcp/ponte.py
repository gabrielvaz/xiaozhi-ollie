"""Ponte MCP do SenseCAP Watcher para o Mac mini.

Expõe ao assistente de voz (xiaozhi-server) ferramentas para:
- herdr: listar, ler, instruir e destravar sessões de agentes (Claude Code, Codex...)
- Claude Code e Codex: perguntas só de leitura e tarefas novas em sessões do herdr
- Multica: issues, agentes e autopilots
- Mac: status e Atalhos (app Shortcuts)

Regra de segurança: toda ferramenta que muda algo exige `confirmado=True`.
Sem isso ela devolve PRECISA_CONFIRMAR e o assistente pergunta antes.
"""

from __future__ import annotations

import json
import os
import re
import shutil
import subprocess
import tempfile
import threading
import time
import uuid
from pathlib import Path

from mcp.server.fastmcp import FastMCP

HOME = Path.home()
os.environ["PATH"] = ":".join(
    [str(HOME / ".local/bin"), "/opt/homebrew/bin", "/usr/local/bin", "/usr/bin", "/bin", "/usr/sbin", "/sbin"]
)

RAIZ_PROJETOS = [Path(p) for p in os.environ.get("PONTE_RAIZES", str(Path.home() / "dev")).split(":")]
ESPERA_SINCRONA_S = int(os.environ.get("PONTE_ESPERA_S", "45"))
LIMITE_TEXTO = 3500
TECLAS_PERMITIDAS = {"enter", "esc", "tab", "up", "down", "left", "right", "y", "n", "1", "2", "3", "4", "ctrl+c"}

mcp = FastMCP("mac-mini")


def _carregar_idioma():
    """idioma.py de extras/core/utils, carregado pelo caminho (a ponte roda fora do pacote core).
    O processo MCP recebe só PONTE_RAIZES e HOME no env: sem IDIOMA, lê a linha IDIOMA= do servidor/.env."""
    if not os.environ.get("IDIOMA"):
        try:
            m = re.search(r"^IDIOMA=(.*)$", (Path(__file__).resolve().parents[1] / ".env").read_text(encoding="utf-8"), re.M)
            if m:
                os.environ["IDIOMA"] = m.group(1).strip().strip('"')
        except OSError:
            pass
    import importlib.util
    spec = importlib.util.spec_from_file_location(
        "idioma", Path(__file__).resolve().parents[1] / "extras/core/utils/idioma.py")
    modulo = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(modulo)
    return modulo


_idioma = _carregar_idioma()
t, NOME = _idioma.t, _idioma.NOME
VOCE = t("Você", "You", "你", "Tú")


# ---------------------------------------------------------------- utilitários

def _run(cmd: list[str], timeout: int = 30, cwd: str | None = None) -> tuple[int, str]:
    try:
        p = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout, cwd=cwd, stdin=subprocess.DEVNULL)
        return p.returncode, (p.stdout or "") + (p.stderr or "")
    except subprocess.TimeoutExpired:
        return 124, f"tempo esgotado após {timeout}s"
    except FileNotFoundError:
        return 127, f"comando não encontrado: {cmd[0]}"


def _json(cmd: list[str], timeout: int = 30) -> dict | list | str:
    code, out = _run(cmd, timeout)
    try:
        return json.loads(out)
    except json.JSONDecodeError:
        return out.strip()[:LIMITE_TEXTO] if code == 0 else f"erro ({code}): {out.strip()[:800]}"


def _corta(texto: str, n: int = LIMITE_TEXTO) -> str:
    texto = re.sub(r"\x1b\[[0-9;?]*[a-zA-Z]", "", texto)
    texto = re.sub(r"[─━│┃┌┐└┘├┤┬┴┼╭╮╯╰═║▐▛▜▝▘▌█]+", " ", texto)
    texto = re.sub(r"[ \t]+", " ", texto)
    texto = re.sub(r"\n\s*\n+", "\n", texto).strip()
    return texto if len(texto) <= n else "…" + texto[-n:]


def _confirmar(confirmado: bool, resumo: str) -> str | None:
    if confirmado:
        return None
    return f"PRECISA_CONFIRMAR: {resumo}. Repita isso ao usuário e só chame de novo com confirmado=true se ele disser sim."


def _projetos() -> dict[str, Path]:
    achados: dict[str, Path] = {}
    for raiz in RAIZ_PROJETOS:
        if not raiz.is_dir():
            continue
        for p in sorted(raiz.iterdir()):
            if p.is_dir() and not p.name.startswith("."):
                achados[p.name] = p
                for sub in sorted(p.iterdir()) if p.is_dir() else []:
                    if sub.is_dir() and (sub / ".git").exists():
                        achados[f"{p.name}/{sub.name}"] = sub
    return achados


def _resolve_projeto(nome: str) -> Path | None:
    if nome and Path(nome).is_dir():
        return Path(nome)
    alvo = re.sub(r"[^a-z0-9]", "", (nome or "").lower())
    if not alvo:
        return None
    candidatos = []
    for chave, caminho in _projetos().items():
        limpo = re.sub(r"[^a-z0-9]", "", chave.lower())
        if limpo == alvo:
            return caminho
        if alvo in limpo:
            candidatos.append((len(limpo), caminho))
    return sorted(candidatos)[0][1] if candidatos else None


PROJETOS_CLAUDE = HOME / ".claude/projects"
SUBAGENTE_ATIVO_S = 180      # subagente cujo histórico mudou nos últimos 3 min está rodando
RECENTE_MIN = 180            # sessões concluídas nas últimas 3 h entram no resumo


def _historico_claude(sessao_id: str, cwd: str) -> dict:
    """Lê o histórico (.jsonl) do Claude Code: última mensagem real, última fala e subagentes ativos."""
    if not sessao_id:
        return {}
    arq = PROJETOS_CLAUDE / re.sub(r"[^A-Za-z0-9]", "-", cwd) / f"{sessao_id}.jsonl"
    if not arq.exists():
        achados = list(PROJETOS_CLAUDE.glob(f"*/{sessao_id}.jsonl"))
        if not achados:
            return {}
        arq = achados[0]
    info = {}
    try:
        with open(arq, "rb") as f:
            f.seek(max(0, arq.stat().st_size - 400_000))   # o final basta
            linhas = f.read().decode("utf-8", errors="ignore").splitlines()
        for linha in reversed(linhas):
            try:
                d = json.loads(linha)
            except json.JSONDecodeError:
                continue
            if d.get("type") not in ("user", "assistant") or d.get("isSidechain"):
                continue
            if "ultima_msg" not in info and d.get("timestamp"):
                info["ultima_msg"] = d["timestamp"]
            conteudo = (d.get("message") or {}).get("content")
            if d.get("type") == "assistant" and isinstance(conteudo, list):
                texto = " ".join(x.get("text", "") for x in conteudo if x.get("type") == "text").strip()
                if texto:
                    info["ultima_fala"] = re.sub(r"[*_`#]", "", texto)[:400]
                    break
    except OSError:
        pass
    agora = time.time()
    if "ultima_msg" in info:
        from datetime import datetime as _dt
        quando = _dt.fromisoformat(info.pop("ultima_msg").replace("Z", "+00:00")).timestamp()
        info["minutos_desde_ultima_atividade"] = int((agora - quando) // 60)
    subs = arq.with_suffix("") / "subagents"
    if subs.is_dir():
        info["subagentes_rodando"] = sum(1 for s in subs.glob("*.jsonl") if agora - s.stat().st_mtime < SUBAGENTE_ATIVO_S)
    return info



def _arquivo_claude(sessao_id: str, cwd: str) -> Path | None:
    if not sessao_id:
        return None
    arq = PROJETOS_CLAUDE / re.sub(r"[^A-Za-z0-9]", "-", cwd) / f"{sessao_id}.jsonl"
    if arq.exists():
        return arq
    achados = list(PROJETOS_CLAUDE.glob(f"*/{sessao_id}.jsonl"))
    return achados[0] if achados else None


def _limpa_msg(texto: str, limite: int) -> str:
    texto = re.sub(r"<system-reminder>.*?</system-reminder>", "", texto, flags=re.S)
    texto = re.sub(r"[ \t]+", " ", texto)
    texto = re.sub(r"\s*\n\s*(\n\s*)*", "\n", texto).strip()
    return texto if len(texto) <= limite else texto[:limite - 1].rstrip() + "…"


def mensagens_claude(sessao_id: str, cwd: str, n: int = 12, limite: int = 500, total: int = 5000) -> list[dict] | None:
    """Últimas mensagens (texto do usuário e do assistente) do JSONL da sessão, da mais antiga para a mais nova."""
    arq = _arquivo_claude(sessao_id, cwd)
    if not arq:
        return None
    from datetime import datetime as _dt
    with open(arq, "rb") as f:
        f.seek(max(0, arq.stat().st_size - 400_000))
        linhas = f.read().decode("utf-8", errors="ignore").splitlines()
    msgs: list[dict] = []
    for linha in reversed(linhas):
        if len(msgs) >= n:
            break
        try:
            d = json.loads(linha)
        except json.JSONDecodeError:
            continue
        tipo = d.get("type")
        if tipo not in ("user", "assistant") or d.get("isSidechain") or d.get("isMeta") or d.get("isCompactSummary"):
            continue
        conteudo = (d.get("message") or {}).get("content")
        if isinstance(conteudo, str):
            texto = conteudo
        elif isinstance(conteudo, list):
            texto = "\n".join(x.get("text", "") for x in conteudo if isinstance(x, dict) and x.get("type") == "text")
        else:
            continue
        texto = _limpa_msg(texto, limite)
        if not texto:
            continue
        if tipo == "user" and (texto.startswith("<") or texto.startswith("Caveat:") or texto.startswith("[Request interrupted")):
            continue   # comandos internos, notificações de tarefa, saídas locais
        hora = ""
        try:
            hora = _dt.fromisoformat(d["timestamp"].replace("Z", "+00:00")).astimezone().strftime("%H:%M")
        except (KeyError, ValueError, AttributeError):
            pass
        msgs.append({"quem": VOCE if tipo == "user" else "Claude", "hora": hora, "texto": texto})
    msgs.reverse()
    while len(msgs) > 1 and sum(len(m["texto"]) for m in msgs) > total:
        msgs.pop(0)
    return msgs


def mensagens_tela(sessao: str, agente: str = "", n: int = 12, limite: int = 500) -> list[dict]:
    """Fallback (Codex e outros): falas marcadas com ⏺/• e prompts › / ❯ do final do terminal no herdr."""
    code, recente = _run(["herdr", "agent", "read", sessao, "--source", "recent-unwrapped", "--lines", "300"])
    if code != 0:
        return []
    quem_ag = {"claude": "Claude", "codex": "Codex"}.get(agente or "", (agente or t("Agente", "Agent", "智能体", "Agente")).capitalize())
    msgs: list[dict] = []
    atual = None
    for l in recente.splitlines():
        m = re.match(r"^\s*(⏺|•|›|❯|>)\s+(.*)", l)
        if m:
            quem = VOCE if m.group(1) in ("›", "❯", ">") else quem_ag
            atual = {"quem": quem, "hora": "", "texto": m.group(2)}
            msgs.append(atual)
        elif atual and l.startswith("  ") and l.strip() and not re.match(r"^\s*[└⎿│]", l):
            atual["texto"] += "\n" + l.strip()
        else:
            atual = None
    msgs = [m for m in msgs if m["texto"].strip() and not re.match(r"^(Ran|Read|Edited|Explored|Bash|Read|Update|Write)\b", m["texto"])]
    for m in msgs:
        m["texto"] = _limpa_msg(m["texto"], limite)
    return msgs[-n:]


def _situacao(a: dict) -> str:
    if a["status"] == "blocked":
        return "esperando você"
    if a["status"] == "working":
        return "trabalhando"
    if a.get("subagentes_rodando"):
        return "subagentes rodando"
    minutos = a.get("minutos_desde_ultima_atividade")
    if minutos is not None and minutos <= RECENTE_MIN:
        return "concluiu recentemente"
    return "parada"


def _agentes() -> list[dict]:
    dados = _json(["herdr", "agent", "list"])
    if not isinstance(dados, dict):
        return []
    rotulos = {}
    ws = _json(["herdr", "workspace", "list"])
    if isinstance(ws, dict):
        for w in ws.get("result", {}).get("workspaces", []):
            rotulos[w["workspace_id"]] = w.get("label", "")
    saida = []
    for a in dados.get("result", {}).get("agents", []):
        item = {
            "sessao": a["pane_id"],
            "agente": a.get("agent"),
            "status": a.get("agent_status"),
            "titulo": a.get("terminal_title_stripped") or "",
            "workspace": rotulos.get(a.get("workspace_id"), ""),
            "pasta": Path(a.get("cwd", "")).name,
            "_conclusoes": a.get("completion_seq") or 0,
        }
        if a.get("agent") == "claude":
            item["_sessao_id"] = (a.get("agent_session") or {}).get("value", "")
            item["_cwd"] = a.get("cwd", "")
            item.update(_historico_claude(item["_sessao_id"], item["_cwd"]))
        item["situacao"] = _situacao(item)
        saida.append(item)
    saida.sort(key=lambda x: x.get("minutos_desde_ultima_atividade", 10**9))
    return saida


# ---------------------------------------------------------------- tarefas longas

_tarefas: dict[str, dict] = {}


def _em_segundo_plano(rotulo: str, cmd: list[str], cwd: str, arquivo_saida: str | None) -> str:
    tid = uuid.uuid4().hex[:6]
    _tarefas[tid] = {"rotulo": rotulo, "status": "rodando", "inicio": time.time(), "resultado": ""}

    def alvo():
        code, out = _run(cmd, timeout=900, cwd=cwd)
        if arquivo_saida and Path(arquivo_saida).exists():
            out = Path(arquivo_saida).read_text() or out
        _tarefas[tid].update(status="ok" if code == 0 else f"erro {code}", resultado=_corta(out))

    t = threading.Thread(target=alvo, daemon=True)
    t.start()
    t.join(ESPERA_SINCRONA_S)
    if not t.is_alive():
        r = _tarefas[tid]
        return f"[{r['status']}] {r['resultado']}"
    return f"AINDA_PENSANDO: tarefa {tid} ({rotulo}). Diga ao usuário que vai demorar e que ele pode perguntar depois; use tarefa_resultado('{tid}')."


@mcp.tool()
def tarefa_resultado(tarefa: str = "") -> str:
    """Resultado de uma pergunta longa ao Claude ou Codex. Sem argumento, lista as tarefas recentes."""
    if not tarefa:
        if not _tarefas:
            return "Nenhuma tarefa."
        return json.dumps(
            [{"tarefa": k, "rotulo": v["rotulo"], "status": v["status"], "ha_s": int(time.time() - v["inicio"])} for k, v in _tarefas.items()],
            ensure_ascii=False,
        )
    r = _tarefas.get(tarefa)
    if not r:
        return "Tarefa não encontrada."
    return f"[{r['status']}] {r['resultado'] or 'ainda rodando'}"


# ---------------------------------------------------------------- herdr

@mcp.tool()
def sessoes_listar(somente_ativas: bool = False) -> str:
    """Lista as sessões de agentes (Claude Code, Codex...) abertas no herdr do Mac mini.
    Cada item traz: sessao (id para as outras ferramentas), agente, status (idle/working/blocked/done), titulo, workspace e pasta.
    blocked = esperando aprovação ou resposta do usuário."""
    ag = _agentes()
    if not ag:
        return "Nenhuma sessão encontrada (o herdr está rodando?)."
    ordem = ("esperando você", "trabalhando", "subagentes rodando", "concluiu recentemente", "parada")
    totais = {s: sum(1 for a in ag if a["situacao"] == s) for s in ordem}
    ag = [{k: v for k, v in a.items() if not k.startswith("_")} for a in ag]
    relevantes = [a for a in ag if a["situacao"] != "parada"]
    if not somente_ativas:
        relevantes += [a for a in ag if a["situacao"] == "parada"][:5]
    return json.dumps({
        "como_relatar": ("Relate CADA sessão que não está parada, pelo título, nesta ordem: primeiro as que esperam "
                         "você (o que pedem), depois as que estão trabalhando ou com subagentes (no que trabalham), "
                         "depois as que concluíram recentemente (o que concluíram, pela ultima_fala resumida em uma "
                         "frase, e há quanto tempo). Das paradas, diga só quantas são. Sem ids."),
        "totais": totais,
        "sessoes": relevantes,
    }, ensure_ascii=False)


@mcp.tool()
def sessao_ler(sessao: str, linhas: int = 80) -> str:
    """Lê o final do terminal de uma sessão do herdr (id tipo 'w3R:p1'). Use para saber o que o agente fez ou está pedindo."""
    linhas = max(20, min(linhas, 300))
    code, visivel = _run(["herdr", "agent", "read", sessao, "--source", "visible"])
    if code != 0:
        return f"erro ao ler: {visivel[:500]}"
    _, recente = _run(["herdr", "agent", "read", sessao, "--source", "recent-unwrapped", "--lines", str(max(linhas, 200))])
    # No Claude Code e no Codex, "⏺"/"•" marcam o que o agente diz ou faz e "✻" o status; o resto costuma ser código
    marcas = re.compile(r"^\s*(⏺|•|✻|✶|✳|✢|·)\s")
    narracao = [l.strip() for l in recente.splitlines() if marcas.match(l)]
    narracao += [l.strip() for l in visivel.splitlines() if marcas.match(l) and l.strip() not in narracao]
    pergunta = [l.strip() for l in visivel.splitlines() if re.search(r"Do you want|❯ \d\.|\(y/n\)|Allow|Permitir", l)]
    partes = []
    if narracao:
        partes.append("O que o agente disse/fez por último:\n" + "\n".join(narracao[-12:]))
    if pergunta:
        partes.append("Pedido na tela:\n" + "\n".join(pergunta[-6:]))
    partes.append("Final da tela:\n" + _corta(visivel, 900))
    return _corta("\n\n".join(partes))


@mcp.tool()
def sessao_instruir(sessao: str, instrucao: str, confirmado: bool = False) -> str:
    """Envia uma instrução (prompt) para uma sessão existente do herdr, como se você digitasse nela.
    Exige confirmado=true depois que o usuário confirmar."""
    pend = _confirmar(confirmado, f"enviar para a sessão {sessao}: “{instrucao}”")
    if pend:
        return pend
    code, out = _run(["herdr", "agent", "prompt", sessao, instrucao], timeout=20)
    return "Instrução enviada." if code == 0 else f"erro: {out[:500]}"


@mcp.tool()
def sessao_responder(sessao: str, teclas: list[str], confirmado: bool = False) -> str:
    """Responde a uma sessão bloqueada (pedido de permissão ou pergunta) apertando teclas.
    Teclas aceitas: enter, esc, tab, up, down, left, right, y, n, 1, 2, 3, 4, ctrl+c.
    Leia a sessão antes (sessao_ler) para saber o que cada opção faz. Exige confirmado=true."""
    ruins = [t for t in teclas if t.lower() not in TECLAS_PERMITIDAS]
    if ruins or not teclas:
        return f"Teclas não permitidas: {ruins or 'nenhuma informada'}."
    pend = _confirmar(confirmado, f"apertar {' '.join(teclas)} na sessão {sessao}")
    if pend:
        return pend
    code, out = _run(["herdr", "agent", "send-keys", sessao, *[t.lower() for t in teclas]])
    return "Teclas enviadas." if code == 0 else f"erro: {out[:500]}"


# ---------------------------------------------------------------- perguntas na tela (AskUserQuestion, permissão)

_RODAPE_PERGUNTA = re.compile(r"esc to cancel|enter to (select|confirm)|to navigate|press enter|esc para cancelar", re.I)
_OPCAO = re.compile(r"^(\s*)(?:[❯›>]\s*)?(\d{1,2})[.)]\s+(.*\S)\s*$")
_CAIXA = re.compile(r"^\[([ ✔✓xX×])\]\s*(.*)$")
_SEPARADOR = re.compile(r"^\s*[─━╌┄═\-]{8,}\s*$")
_ABAS = re.compile(r"[☐☒✔]\s*Submit|^\s*←?\s*[☐☒]\s+\S")
_LIVRE = re.compile(r"^(type something|other|outro|digite algo)\.?$", re.I)
_IGNORAR = re.compile(r"^(chat about this)\.?$", re.I)


def _parse_pergunta(tela: str) -> dict | None:
    """Acha uma pergunta com opções numeradas no texto visível de um painel (AskUserQuestion do Claude Code,
    pedido de permissão "Do you want to proceed?", aprovação do Codex). Devolve
    {"texto", "multipla", "opcoes": [{"n", "texto", "descricao"?, "marcada"?, "livre"?}], "contexto"?, "abas"?} ou None."""
    linhas = [re.sub(r"\x1b\[[0-9;?]*[a-zA-Z]", "", l).rstrip() for l in (tela or "").splitlines()]
    while linhas and not linhas[-1].strip():
        linhas.pop()
    if not linhas:
        return None
    # Rodapé de diálogo ("Esc to cancel", "Enter to select"...) nas últimas linhas: sem ele não é pergunta aberta
    nao_vazias = [i for i, l in enumerate(linhas) if l.strip()]
    rodape = next((i for i in reversed(nao_vazias[-6:]) if _RODAPE_PERGUNTA.search(linhas[i])), None)
    if rodape is None:
        # Às vezes o rodapé ainda não foi desenhado: aceita se a tela termina nas opções (o caso de "1." logo abaixo de "...?")
        if not _OPCAO.match(linhas[-1]):
            return None
        rodape = len(linhas)
    # Última opção "1." antes do rodapé seguida de 2, 3... em sequência
    inicio = None
    for i in range(rodape - 1, -1, -1):
        m = _OPCAO.match(linhas[i])
        if m and m.group(2) == "1":
            inicio = i
            break
    if inicio is None or rodape - inicio > 60:
        return None
    opcoes: list[dict] = []
    atual = None
    for l in linhas[inicio:rodape]:
        m = _OPCAO.match(l)
        if m and int(m.group(2)) == len(opcoes) + 1:
            atual = {"n": int(m.group(2)), "texto": m.group(3).strip()}
            opcoes.append(atual)
        elif atual is not None and l.strip() and not _SEPARADOR.match(l):
            extra = l.strip()
            if extra.lower() not in ("next", "submit"):
                atual["descricao"] = (atual.get("descricao", "") + " " + extra).strip()
        elif _SEPARADOR.match(l):
            atual = None
    if len(opcoes) < 2:
        return None
    multipla = any(_CAIXA.match(o["texto"]) for o in opcoes)
    finais = []
    for o in opcoes:
        cx = _CAIXA.match(o["texto"])
        if cx:
            o["texto"] = cx.group(2).strip()
            if multipla:
                o["marcada"] = cx.group(1) != " "
        if _IGNORAR.match(o["texto"]):
            continue
        if o.get("descricao") == o["texto"]:
            o.pop("descricao")
        if _LIVRE.match(o["texto"]):
            o["livre"] = True
            o.pop("descricao", None)
        o["texto"] = o["texto"].replace("’", "'")
        finais.append(o)
    # Pergunta: linhas não vazias logo acima da opção 1; contexto: o que vem acima até o separador ─ (comando, revisão...)
    acima = []
    j = inicio - 1
    while j >= 0 and not linhas[j].strip():
        j -= 1
    while j >= 0 and linhas[j].strip() and not _SEPARADOR.match(linhas[j]) and not _ABAS.search(linhas[j]):
        acima.insert(0, linhas[j].strip())
        j -= 1
    contexto, abas = [], []
    while j >= 0 and not re.match(r"^\s*─{8,}", linhas[j]) and inicio - j < 40:
        l = linhas[j].strip()
        if _ABAS.search(linhas[j]):
            abas = [{"nome": n.strip(), "respondida": s == "☒"} for s, n in re.findall(r"([☐☒])\s+([^☐☒✔→]+)", l)]
        elif l and not _SEPARADOR.match(linhas[j]) and not l.lower().startswith("tip:"):
            contexto.insert(0, l)
        j -= 1
    texto = " ".join(acima) or t("Pergunta da sessão", "Session question", "会话提问", "Pregunta de la sesión")
    if rodape == len(linhas) and not texto.endswith("?"):
        return None
    r: dict = {"texto": texto[:400], "multipla": multipla, "opcoes": finais}
    if contexto:
        r["contexto"] = " · ".join(contexto)[:400]
    if abas:
        r["abas"] = abas
    return r


def pergunta_tela(sessao: str) -> dict | None:
    """Pergunta aberta na tela da sessão (None se não houver ou se a sessão não existir)."""
    code, visivel = _run(["herdr", "agent", "read", sessao, "--source", "visible"])
    return _parse_pergunta(visivel) if code == 0 else None


def _teclas(sessao: str, *teclas: str) -> bool:
    code, _ = _run(["herdr", "agent", "send-keys", sessao, *teclas], timeout=15)
    return code == 0


def _ir_para(sessao: str, n: int) -> bool:
    """Opção com número de 2 dígitos: navega com setas a partir da primeira e aperta enter."""
    return _teclas(sessao, *(["up"] * 30), *(["down"] * (n - 1)), "enter")


def _espera_mudar(sessao: str, antes: dict | None, segundos: float = 4.0) -> dict | None:
    """Relê a tela até a pergunta sumir ou mudar. Devolve a pergunta atual (None = sumiu)."""
    fim = time.time() + segundos
    agora = antes
    while time.time() < fim:
        time.sleep(0.6)
        agora = pergunta_tela(sessao)
        if agora is None or antes is None or agora.get("texto") != antes.get("texto") or agora.get("opcoes") != antes.get("opcoes"):
            return agora
    return agora


def _revisao(p: dict | None) -> bool:
    """Tela final do AskUserQuestion com várias abas ("Ready to submit your answers?"), com todas respondidas."""
    return bool(p and re.search(r"submit your answers", p["texto"], re.I)
                and all(a.get("respondida") for a in p.get("abas", [])))


def responder_pergunta(sessao: str, escolhas: list[int]) -> dict:
    """Seleciona as opções na pergunta aberta e confirma. Devolve {"ok", "mensagem", "proxima"?}."""
    p = pergunta_tela(sessao)
    if p is None:
        return {"ok": False, "mensagem": t("Não há pergunta aberta nessa sessão.", "There is no open question in this session.",
                                             "这个会话没有待回答的问题。", "No hay ninguna pregunta abierta en esta sesión.")}
    try:
        escolhas = sorted({int(e) for e in escolhas})
    except (TypeError, ValueError):
        return {"ok": False, "mensagem": t("Escolhas inválidas (use números).", "Invalid choices (use numbers).",
                                             "选项无效（请使用数字）。", "Opciones no válidas (usa números).")}
    por_n = {o["n"]: o for o in p["opcoes"]}
    if not escolhas:
        return {"ok": False, "mensagem": t("Nenhuma opção escolhida.", "No option chosen.", "没有选择任何选项。",
                                             "No se eligió ninguna opción.")}
    fora = [e for e in escolhas if e not in por_n]
    if fora:
        return {"ok": False, "mensagem": t("Opção inexistente", "No such option", "选项不存在", "Opción inexistente") + f": {', '.join(map(str, fora))}."}
    if any(por_n[e].get("livre") for e in escolhas):
        return {"ok": False, "mensagem": t("Resposta livre só pela própria sessão (digitando).",
                                             "Free-text answers only in the session itself (typing).",
                                             "自由回答只能在会话中输入。",
                                             "Respuesta libre solo en la propia sesión (escribiendo).")}
    if not p["multipla"] and len(escolhas) > 1:
        return {"ok": False, "mensagem": t("Essa pergunta aceita uma opção só.", "This question accepts only one option.",
                                             "这个问题只能选一个选项。", "Esta pregunta solo acepta una opción.")}
    nomes = ", ".join(por_n[e]["texto"] for e in escolhas)
    if p["multipla"]:
        # Número alterna a caixa sem mover o cursor; só alterna o que difere do desejado. Depois tab = Next/confirmar.
        for o in p["opcoes"]:
            if o.get("livre"):
                continue
            if (o["n"] in escolhas) != bool(o.get("marcada")):
                if o["n"] > 9 or not _teclas(sessao, str(o["n"])):
                    return {"ok": False, "mensagem": t("Não consegui marcar as opções.", "Couldn't select the options.",
                                                     "无法勾选这些选项。", "No pude marcar las opciones.")}
                time.sleep(0.15)
        ok = _teclas(sessao, "tab")
    else:
        n = escolhas[0]
        ok = _teclas(sessao, str(n)) if n <= 9 else _ir_para(sessao, n)
    if not ok:
        return {"ok": False, "mensagem": t("Não consegui enviar as teclas à sessão.", "Couldn't send the keys to the session.",
                                             "无法向会话发送按键。", "No pude enviar las teclas a la sesión.")}
    depois = _espera_mudar(sessao, p)
    if _revisao(depois):  # várias perguntas: última respondida, envia a revisão
        _teclas(sessao, "1")
        depois = _espera_mudar(sessao, depois)
    if depois is not None and depois.get("texto") == p.get("texto") and depois.get("opcoes") == p.get("opcoes"):
        return {"ok": False, "mensagem": t(f"Enviei {nomes}, mas a pergunta continua na tela.",
                                             f"Sent {nomes}, but the question is still on screen.",
                                             f"已发送 {nomes}，但问题仍在屏幕上。",
                                             f"Envié {nomes}, pero la pregunta sigue en pantalla.")}
    r = {"ok": True, "mensagem": t("Respondido", "Answered", "已回答", "Respondido") + f": {nomes}"}
    if depois is not None:
        r["proxima"] = depois
        r["mensagem"] += ". " + t("Próxima pergunta", "Next question", "下一个问题", "Siguiente pregunta") + f": {depois['texto']}"
    return r


@mcp.tool()
def sessao_pergunta(sessao: str) -> str:
    """Pergunta com opções que a sessão do herdr está mostrando (AskUserQuestion do Claude Code ou pedido de permissão).
    Leia ao usuário a pergunta e as opções pelo número; multipla=true aceita várias. Depois use sessao_escolher."""
    p = pergunta_tela(sessao)
    if p is None:
        return "Nenhuma pergunta com opções aberta nessa sessão. Use sessao_ler para ver a tela."
    return json.dumps({"como_relatar": "Leia a pergunta e cada opção com o número. Opções livre=true só digitando na sessão.",
                       "pergunta": p}, ensure_ascii=False)


@mcp.tool()
def sessao_escolher(sessao: str, escolhas: list[int], confirmado: bool = False) -> str:
    """Responde à pergunta aberta na sessão escolhendo as opções pelo número (ex.: [2] ou [1, 3] se for múltipla).
    Use sessao_pergunta antes. Exige confirmado=true depois que o usuário confirmar."""
    p = pergunta_tela(sessao)
    if p is None:
        return "Nenhuma pergunta com opções aberta nessa sessão."
    nomes = {o["n"]: o["texto"] for o in p["opcoes"]}
    resumo = ", ".join(f"{e} ({nomes.get(e, '?')})" for e in escolhas) if escolhas else "nada"
    pend = _confirmar(confirmado, f"responder “{p['texto']}” na sessão {sessao} com {resumo}")
    if pend:
        return pend
    return json.dumps(responder_pergunta(sessao, escolhas), ensure_ascii=False)


def _nova_sessao(tipo: str, projeto: str, instrucao: str) -> str:
    pasta = _resolve_projeto(projeto)
    if not pasta:
        return f"Projeto '{projeto}' não encontrado. Use projetos_listar."
    nome = f"watcher-{tipo}-{uuid.uuid4().hex[:4]}"
    ws = _json(["herdr", "workspace", "create", "--cwd", str(pasta), "--label", nome, "--no-focus"])
    try:
        pane = ws["result"]["root_pane"]["pane_id"]
    except (TypeError, KeyError):
        return f"erro ao criar workspace: {str(ws)[:400]}"
    code, out = _run(["herdr", "agent", "start", nome, "--kind", tipo, "--pane", pane, "--timeout", "60000"], timeout=70)
    if code != 0:
        return f"erro ao iniciar {tipo}: {out[:400]}"
    code, out = _run(["herdr", "agent", "prompt", nome, instrucao], timeout=20)
    if code != 0:
        return f"{tipo} iniciado em {pane}, mas o prompt falhou: {out[:300]}"
    return f"Sessão {nome} ({pane}) criada em {pasta.name} e trabalhando. Acompanhe com sessao_ler('{pane}')."


@mcp.tool()
def claude_nova_sessao(projeto: str, instrucao: str, confirmado: bool = False) -> str:
    """Abre uma sessão NOVA do Claude Code no herdr, na pasta do projeto, e manda a instrução.
    Para trabalho de verdade (editar, rodar, criar PR). Exige confirmado=true."""
    pend = _confirmar(confirmado, f"abrir Claude Code em '{projeto}' com a tarefa “{instrucao}”")
    return pend or _nova_sessao("claude", projeto, instrucao)


@mcp.tool()
def codex_nova_sessao(projeto: str, instrucao: str, confirmado: bool = False) -> str:
    """Abre uma sessão NOVA do Codex no herdr, na pasta do projeto, e manda a instrução. Exige confirmado=true."""
    pend = _confirmar(confirmado, f"abrir Codex em '{projeto}' com a tarefa “{instrucao}”")
    return pend or _nova_sessao("codex", projeto, instrucao)


@mcp.tool()
def codex_enviar(sessao: str, texto: str, confirmado: bool = False) -> str:
    """Manda uma mensagem para uma sessão do Codex do Mac (id mostrado no app Codex do Watcher, não é painel
    do herdr). Se ela estiver trabalhando, entra na fila; se estiver parada, o Codex retoma em modo só leitura.
    Exige confirmado=true."""
    pend = _confirmar(confirmado, f"mandar ao Codex “{texto}”")
    if pend:
        return pend
    r = _codex_remoto().enviar(sessao, texto)
    return r.get("mensagem") or ("Enviado." if r.get("ok") else "Não consegui enviar.")


def _codex_remoto():
    """Módulo extras/core/utils/codex_remoto.py (só biblioteca padrão), carregado pelo caminho."""
    import importlib.util
    caminho = Path(__file__).resolve().parents[1] / "extras/core/utils/codex_remoto.py"
    spec = importlib.util.spec_from_file_location("codex_remoto", caminho)
    modulo = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(modulo)
    return modulo


# ---------------------------------------------------------------- perguntas só de leitura

@mcp.tool()
def claude_perguntar(pergunta: str, projeto: str = "") -> str:
    """Pergunta ao Claude Code do Mac (com skills e memória do usuário) sobre um projeto, SEM alterar nada.
    Bom para: 'como está o PR?', 'o que falta no projeto X?', 'resuma o último commit'. Pode levar até alguns minutos."""
    pasta = _resolve_projeto(projeto) if projeto else RAIZ_PROJETOS[0]
    if not pasta:
        return f"Projeto '{projeto}' não encontrado. Use projetos_listar."
    cmd = [
        "claude", "-p", pergunta + f"\n\nResponda em {NOME}, em no máximo 5 frases curtas, para ser lido em voz alta. Sem markdown.",
        "--permission-mode", "default",
        "--disallowedTools", "Edit,Write,NotebookEdit,Bash(rm:*),Bash(git push:*),Bash(git commit:*)",
        "--allowedTools", "Read,Grep,Glob,LS,WebSearch,WebFetch,Bash(git status:*),Bash(git log:*),Bash(git diff:*),Bash(gh pr view:*),Bash(gh pr list:*),Bash(gh pr checks:*)",
    ]
    return _em_segundo_plano(f"claude: {pergunta[:40]}", cmd, str(pasta), None)


@mcp.tool()
def codex_perguntar(pergunta: str, projeto: str = "") -> str:
    """Pergunta ao Codex do Mac sobre um projeto, em modo só leitura (sandbox read-only)."""
    pasta = _resolve_projeto(projeto) if projeto else RAIZ_PROJETOS[0]
    if not pasta:
        return f"Projeto '{projeto}' não encontrado. Use projetos_listar."
    saida = tempfile.mktemp(prefix="codex-", suffix=".txt")
    cmd = [
        "codex", "exec", "--sandbox", "read-only", "--skip-git-repo-check", "--ephemeral", "-C", str(pasta), "-o", saida,
        pergunta + f"\n\nResponda em {NOME}, em no máximo 5 frases curtas, para ser lido em voz alta. Sem markdown.",
    ]
    return _em_segundo_plano(f"codex: {pergunta[:40]}", cmd, str(pasta), saida)


@mcp.tool()
def projetos_listar(filtro: str = "") -> str:
    """Lista as pastas de projeto conhecidas no Mac (para usar em claude_perguntar, codex_perguntar e *_nova_sessao)."""
    nomes = [n for n in _projetos() if filtro.lower() in n.lower()]
    return ", ".join(nomes[:80]) or "Nenhum projeto."


# ---------------------------------------------------------------- Multica

def _multica(*args: str, timeout: int = 30):
    return _json(["multica", *args], timeout)


@mcp.tool()
def multica_status() -> str:
    """Status do Multica: login, daemon local e runtimes."""
    _, auth = _run(["multica", "auth", "status"], 15)
    _, daemon = _run(["multica", "daemon", "status"], 15)
    return _corta(f"auth: {auth}\ndaemon: {daemon}", 1500)


@mcp.tool()
def multica_issues(busca: str = "", issue: str = "") -> str:
    """Lista issues do Multica (opcionalmente filtrando por texto) ou detalha uma issue (ex.: 'MUL-12'), com runs."""
    if issue:
        dados = {"issue": _multica("issue", "get", issue, "--output", "json"), "runs": _multica("issue", "runs", issue, "--output", "json")}
    elif busca:
        dados = _multica("issue", "search", busca, "--output", "json")
    else:
        dados = _multica("issue", "list", "--output", "json")
    return _corta(json.dumps(dados, ensure_ascii=False))


@mcp.tool()
def multica_agentes() -> str:
    """Lista os agentes do Multica e seus runtimes."""
    return _corta(json.dumps({"agentes": _multica("agent", "list", "--output", "json"), "runtimes": _multica("runtime", "list", "--output", "json")}, ensure_ascii=False))


@mcp.tool()
def multica_criar_issue(titulo: str, descricao: str = "", agente: str = "", confirmado: bool = False) -> str:
    """Cria uma issue no Multica, opcionalmente já atribuída a um agente (que começa a trabalhar). Exige confirmado=true."""
    pend = _confirmar(confirmado, f"criar issue “{titulo}”" + (f" para {agente}" if agente else ""))
    if pend:
        return pend
    args = ["issue", "create", "--title", titulo, "--output", "json"]
    if descricao:
        args += ["--description", descricao]
    if agente:
        args += ["--assignee", agente]
    return _corta(json.dumps(_multica(*args), ensure_ascii=False), 1500)


@mcp.tool()
def multica_comentar(issue: str, texto: str, confirmado: bool = False) -> str:
    """Comenta numa issue do Multica (o agente atribuído lê e reage). Exige confirmado=true."""
    pend = _confirmar(confirmado, f"comentar na {issue}: “{texto}”")
    if pend:
        return pend
    code, out = _run(["multica", "issue", "comment", "add", issue, "--content", texto])
    return "Comentário enviado." if code == 0 else f"erro: {out[:500]}"


@mcp.tool()
def multica_mudar_status(issue: str, status: str, confirmado: bool = False) -> str:
    """Muda a situação de uma issue do Multica (ex.: 'MDB-1'). status: backlog, todo, in_progress, in_review, done,
    blocked ou cancelled. Exige confirmado=true."""
    pend = _confirmar(confirmado, f"mudar a {issue} para {status}")
    if pend:
        return pend
    code, out = _run(["multica", "issue", "status", issue, status])
    return "Situação alterada." if code == 0 else f"erro: {out[:500]}"


@mcp.tool()
def multica_daemon(ligar: bool, confirmado: bool = False) -> str:
    """Liga ou desliga o daemon local do Multica (com ele ligado, os agentes deste Mac pegam tarefas). Exige confirmado=true."""
    pend = _confirmar(confirmado, "ligar o daemon do Multica" if ligar else "desligar o daemon do Multica")
    if pend:
        return pend
    code, out = _run(["multica", "daemon", "start" if ligar else "stop"], 40)
    return ("Daemon ligado." if ligar else "Daemon desligado.") if code == 0 else f"erro: {out[:500]}"


@mcp.tool()
def multica_autopilot(autopilot: str = "", disparar: bool = False, confirmado: bool = False) -> str:
    """Sem argumentos lista os autopilots do Multica. Com autopilot e disparar=true, executa um agora (exige confirmado=true)."""
    if not disparar:
        if autopilot:
            return _corta(json.dumps(_multica("autopilot", "runs", autopilot, "--output", "json"), ensure_ascii=False))
        return _corta(json.dumps(_multica("autopilot", "list", "--output", "json"), ensure_ascii=False))
    pend = _confirmar(confirmado, f"disparar o autopilot {autopilot}")
    if pend:
        return pend
    code, out = _run(["multica", "autopilot", "trigger", autopilot])
    return "Autopilot disparado." if code == 0 else f"erro: {out[:500]}"


# ---------------------------------------------------------------- uso do Claude (limites de 5 h e 7 dias)

_uso_cache = {"quando": 0.0, "dados": None}


def uso_claude_dados(idade_max_s: int = 120) -> dict:
    """Lê os limites do plano com uma chamada mínima (max_tokens=1) usando o token OAuth do Claude Code
    guardado no Keychain; a resposta traz os cabeçalhos anthropic-ratelimit-unified-*.
    Mesma técnica do projeto watcher-claude-usage, mas o token não sai do Mac."""
    if _uso_cache["dados"] and time.time() - _uso_cache["quando"] < idade_max_s:
        return _uso_cache["dados"]
    code, saida = _run(["security", "find-generic-password", "-s", "Claude Code-credentials", "-w"], 10)
    if code != 0:
        return {"ok": False, "erro": t("Credencial do Claude Code não encontrada no Keychain.",
                                         "Claude Code credential not found in the Keychain.",
                                         "钥匙串中没有找到 Claude Code 凭据。",
                                         "No se encontró la credencial de Claude Code en el Llavero.")}
    try:
        token = json.loads(saida.strip())["claudeAiOauth"]["accessToken"]
    except (json.JSONDecodeError, KeyError, TypeError):
        return {"ok": False, "erro": t("Credencial do Claude Code em formato inesperado.",
                                         "Claude Code credential in an unexpected format.",
                                         "Claude Code 凭据格式异常。",
                                         "Credencial de Claude Code con un formato inesperado.")}
    import urllib.request
    _, versao = _run(["claude", "--version"], 10)
    req = urllib.request.Request(
        "https://api.anthropic.com/v1/messages", method="POST",
        data=json.dumps({"model": "claude-haiku-4-5-20251001", "max_tokens": 1,
                         "messages": [{"role": "user", "content": "hi"}]}).encode(),
        headers={"Authorization": f"Bearer {token}", "anthropic-beta": "oauth-2025-04-20",
                 "anthropic-version": "2023-06-01", "content-type": "application/json",
                 "User-Agent": f"claude-code/{versao.split()[0] if versao.strip() else '2.0.0'}"})
    try:
        with urllib.request.urlopen(req, timeout=15) as r:
            h = r.headers
    except urllib.error.HTTPError as e:
        if e.code == 401:
            return {"ok": False, "erro": t("Token do Claude Code expirado. Abra o Claude Code no Mac para renovar.",
                                             "Claude Code token expired. Open Claude Code on the Mac to renew it.",
                                             "Claude Code 令牌已过期。请在 Mac 上打开 Claude Code 以续期。",
                                             "El token de Claude Code caducó. Abre Claude Code en el Mac para renovarlo.")}
        h = e.headers  # 429 também traz os cabeçalhos de limite
    def pct(nome):
        v = h.get(f"anthropic-ratelimit-unified-{nome}-utilization")
        return round(float(v) * 100) if v else None
    def reinicio(nome):
        v = h.get(f"anthropic-ratelimit-unified-{nome}-reset")
        if not v:
            return None
        resta = int(float(v) - time.time())
        return max(0, resta)
    dados = {"ok": pct("5h") is not None or pct("7d") is not None,
             "pct_5h": pct("5h"), "pct_7d": pct("7d"),
             "reinicio_5h_s": reinicio("5h"), "reinicio_7d_s": reinicio("7d"),
             "status": h.get("anthropic-ratelimit-unified-status", "")}
    if not dados["ok"]:
        dados["erro"] = t("A API não devolveu os limites.", "The API did not return the limits.",
                          "API 没有返回用量上限。", "La API no devolvió los límites.")
    _uso_cache.update(quando=time.time(), dados=dados)
    return dados


@mcp.tool()
def uso_claude() -> str:
    """Quanto dos limites do plano do Claude (janela de 5 horas e de 7 dias) já foi usado e quando reiniciam."""
    d = uso_claude_dados()
    if not d.get("ok"):
        return d.get("erro", "Não consegui ler o uso agora.")
    def tempo(s):
        if s is None:
            return "?"
        return f"{s // 3600} h {s % 3600 // 60} min" if s >= 3600 else f"{s // 60} min"
    return (f"Janela de 5 h: {d['pct_5h']}% usado, reinicia em {tempo(d['reinicio_5h_s'])}. "
            f"Semana: {d['pct_7d']}% usado, reinicia em {tempo(d['reinicio_7d_s'])}. Status: {d['status']}.")


# ---------------------------------------------------------------- lembretes e notas

LEMBRETES = Path(os.environ.get("WATCHER_LEMBRETES",
                                HOME / "Library/Mobile Documents/com~apple~CloudDocs/Watcher/Lembretes.md"))


def _ler_icloud(caminho: Path) -> str:
    """O iCloud devolve errno 11 (EDEADLK) enquanto sincroniza o arquivo: tenta de novo."""
    import errno
    for i in range(8):
        try:
            return caminho.read_text(encoding="utf-8")
        except OSError as e:
            if e.errno != errno.EDEADLK or i == 7:
                raise
            time.sleep(0.4)
    return ""


def _ler_lembretes() -> list[str]:
    if not LEMBRETES.exists():
        return []
    return [l[2:].strip() for l in _ler_icloud(LEMBRETES).splitlines() if l.startswith("- ")]


def _gravar_lembretes(itens: list[str]) -> None:
    LEMBRETES.parent.mkdir(parents=True, exist_ok=True)
    LEMBRETES.write_text("# Lembretes do Watcher\n\n" + "".join(f"- {i}\n" for i in itens), encoding="utf-8")


@mcp.tool()
def lembrete_salvar(texto: str) -> str:
    """Guarda um lembrete ou nota quando o usuário diz "lembra que...", "anota que..." ou "guarda isso".
    Fica disponível também na memória offline do Watcher."""
    from datetime import datetime as _dt
    itens = _ler_lembretes()
    itens.append(f"{_dt.now():%d/%m %H:%M} · {texto.strip()}")
    _gravar_lembretes(itens[-50:])
    return "Lembrete guardado."


@mcp.tool()
def lembretes_listar() -> str:
    """Lista os lembretes e notas guardados (mais recentes por último)."""
    itens = _ler_lembretes()
    return "\n".join(f"{n}. {i}" for n, i in enumerate(itens, 1)) or "Nenhum lembrete guardado."


@mcp.tool()
def lembrete_apagar(numero: int, confirmado: bool = False) -> str:
    """Apaga um lembrete pelo número mostrado em lembretes_listar. Exige confirmado=true."""
    itens = _ler_lembretes()
    if not 1 <= numero <= len(itens):
        return "Número de lembrete inválido."
    pend = _confirmar(confirmado, f"apagar o lembrete “{itens[numero - 1]}”")
    if pend:
        return pend
    del itens[numero - 1]
    _gravar_lembretes(itens)
    return "Lembrete apagado."


# ---------------------------------------------------------------- conversas antigas (memória)

CONVERSAS = LEMBRETES.parent / "Conversas"


@mcp.tool()
def conversas_buscar(termo: str = "", dias: int = 30) -> str:
    """Procura nas conversas antigas com o Ollie (memória de longo prazo). Use quando o usuário perguntar
    "o que eu te falei sobre X", "do que a gente conversou ontem" ou pedir algo dito antes. termo vazio
    lista as conversas dos últimos `dias` com título e resumo; com termo, devolve os trechos que o citam."""
    try:
        indice = json.loads(_ler_icloud(CONVERSAS / "indice.json"))
    except (OSError, json.JSONDecodeError):
        return "Ainda não há conversas registradas."
    from datetime import datetime, timedelta
    limite = (datetime.now() - timedelta(days=max(1, dias))).isoformat()
    recentes = sorted((c for c in indice if c["inicio"] >= limite), key=lambda c: c["inicio"], reverse=True)
    if not termo.strip():
        linhas = [f"{c['inicio'][:16].replace('T', ' ')} · {c.get('titulo') or 'em andamento'}: {c.get('resumo', '')}"
                  for c in recentes[:25]]
        return "\n".join(linhas) or f"Nenhuma conversa nos últimos {dias} dias."
    palavras = [w for w in re.findall(r"\w+", termo.lower()) if len(w) > 2] or [termo.lower()]
    achados = []
    for c in recentes:
        try:
            texto = _ler_icloud(CONVERSAS / c["arquivo"])
        except OSError:
            continue
        falas = [l for l in texto.splitlines() if l.startswith("**") or l.startswith("> ")]
        trechos = [l.replace("**", "") for l in falas if any(w in l.lower() for w in palavras)]
        if trechos:
            achados.append(f"## {c['inicio'][:16].replace('T', ' ')} · {c.get('titulo') or 'Conversa'}\n"
                           + "\n".join(t[:400] for t in trechos[:6]))
        if len(achados) >= 6:
            break
    return "\n\n".join(achados) or f"Não achei “{termo}” nas conversas dos últimos {dias} dias."


# ---------------------------------------------------------------- Mac

@mcp.tool()
def mac_status() -> str:
    """Status do Mac: tempo ligado, carga, memória livre e espaço em disco."""
    _, up = _run(["uptime"])
    _, disco = _run(["df", "-h", *os.environ.get("WATCHER_DISCOS", "/").split(":")])
    _, mem = _run(["memory_pressure", "-Q"])
    return _corta(f"{up}\n{disco}\n{mem}", 1500)


@mcp.tool()
def mac_atalhos_listar() -> str:
    """Lista os Atalhos (app Shortcuts) do Mac que podem ser executados."""
    _, out = _run(["shortcuts", "list"])
    return _corta(out, 2500)


@mcp.tool()
def mac_atalho_executar(nome: str, entrada: str = "", confirmado: bool = False) -> str:
    """Executa um Atalho do macOS pelo nome exato (veja mac_atalhos_listar). É o jeito de fazer ações no Mac. Exige confirmado=true."""
    pend = _confirmar(confirmado, f"executar o atalho “{nome}” no Mac")
    if pend:
        return pend
    cmd = ["shortcuts", "run", nome]
    if entrada:
        arq = tempfile.mktemp(suffix=".txt")
        Path(arq).write_text(entrada)
        cmd += ["--input-path", arq]
    code, out = _run(cmd, timeout=60)
    return f"Atalho executado. {out.strip()[:500]}" if code == 0 else f"erro: {out[:500]}"


@mcp.tool()
def mac_notificar(mensagem: str) -> str:
    """Mostra uma notificação na tela do Mac mini."""
    msg = mensagem.replace('"', "'")[:200]
    code, out = _run(["osascript", "-e", f'display notification "{msg}" with title "SenseCAP Watcher"'])
    return "Notificação exibida." if code == 0 else f"erro: {out[:300]}"


if __name__ == "__main__":
    faltando = [c for c in ("herdr", "claude", "codex", "multica", "shortcuts") if not shutil.which(c)]
    if faltando:
        import sys
        print(f"aviso: comandos ausentes no PATH: {faltando}", file=sys.stderr)
    mcp.run()

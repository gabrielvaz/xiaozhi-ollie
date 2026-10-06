"""Vigia das sessões: gera avisos para a tela do Watcher.

A cada 10 s olha o herdr (pela ponte) e detecta:
  - sessão passou a esperar você (permissão ou pergunta);
  - sessão terminou de trabalhar (com o resumo do que concluiu).
Outros módulos (ex.: reuniões) chamam adicionar_aviso. O Watcher busca em GET /watcher/avisos.
Todos os avisos ficam também no histórico (data/avisos_historico.json, os 200 mais recentes),
lido pelo app Avisos do Watcher em GET /watcher/avisos/historico.
"""

import os
import sys
import threading
import time
from pathlib import Path

import requests

from core.utils.idioma import IDIOMA, NOME, t

INTERVALO_S = 10
MAX_AVISOS = 50
MAX_HISTORICO = 200
HISTORICO = Path(__file__).resolve().parents[2] / "data/avisos_historico.json"
_avisos: list[dict] = []
_atividade: dict = {"trabalhando": 0, "titulos": []}
_sessoes = {"quando": 0.0, "lista": []}  # lista compacta para o app Claude Code (vai junto com os avisos)


def sessoes_compactas(agentes: list[dict]) -> list[dict]:
    """Lista do app Claude Code do Watcher: situação (em português: o firmware compara), há quanto tempo e a última fala."""
    rotulos = {"esperando você": "Esperando você", "trabalhando": "Trabalhando", "subagentes rodando": "Subagentes",
               "concluiu recentemente": "Concluída", "parada": "Parada"}
    sessao_padrao = t("Sessão", "Session", "会话", "Sesión")
    un_min, un_h = t("min", "min", "分钟", "min"), t("h", "h", "小时", "h")
    sessoes = []
    for a in agentes:
        if a["situacao"] == "parada" and len(sessoes) >= 12:
            continue
        minutos = a.get("minutos_desde_ultima_atividade")
        sessoes.append({
            "id": a["sessao"],
            "titulo": (a.get("titulo") or a.get("workspace") or a.get("pasta") or sessao_padrao)[:40],
            "agente": a.get("agente", ""),
            "situacao": rotulos.get(a["situacao"], a["situacao"]),
            "ha": "" if minutos is None else (f"{minutos} {un_min}" if minutos < 60 else f"{minutos // 60} {un_h}"),
            "ultima": (a.get("ultima_fala") or "")[:300],
        })
    return sessoes[:20]


def sessoes_recentes(idade_max_s: float = 15) -> list[dict] | None:
    """A lista calculada pelo vigia, se tiver no máximo idade_max_s segundos."""
    with _trava:
        return list(_sessoes["lista"]) if time.time() - _sessoes["quando"] <= idade_max_s else None
_seq = 0
_trava = threading.Lock()
_iniciado = False


def adicionar_aviso(tipo: str, titulo: str, texto: str, emocao: str = "neutral", sessao: str = "",
                    nome_sessao: str = "") -> None:
    """sessao = id do painel no herdr (o mesmo de /watcher/sessoes); com tipo "esperando" o Watcher abre a sessão."""
    global _seq
    with _trava:
        _seq += 1
        aviso = {"id": _seq, "tipo": tipo, "titulo": titulo[:40], "texto": texto[:110], "emocao": emocao,
                 "quando": int(time.time())}
        if sessao:
            aviso["sessao"] = sessao
        if nome_sessao:  # o app Avisos usa como título (vários "Tarefa concluída" ficavam iguais)
            aviso["nome_sessao"] = nome_sessao[:50]
        _avisos.append(aviso)
        del _avisos[:-MAX_AVISOS]
        _guardar_historico(aviso)


def _guardar_historico(aviso: dict) -> None:
    import json
    try:
        itens = json.loads(HISTORICO.read_text(encoding="utf-8")) if HISTORICO.exists() else []
    except (OSError, ValueError):
        itens = []
    itens.append(aviso)
    HISTORICO.parent.mkdir(parents=True, exist_ok=True)
    tmp = HISTORICO.with_suffix(".tmp")
    tmp.write_text(json.dumps(itens[-MAX_HISTORICO:], ensure_ascii=False), encoding="utf-8")
    tmp.replace(HISTORICO)


DIAS_SEMANA = t("Seg Ter Qua Qui Sex Sáb Dom", "Mon Tue Wed Thu Fri Sat Sun", "周一 周二 周三 周四 周五 周六 周日",
                "Lun Mar Mié Jue Vie Sáb Dom").split()


def rotulo_dia(d) -> str:
    """Dia curto para listas da tela: "Hoje", "Ontem" ou "Seg 05/10" (mês/dia em en-US e zh-CN)."""
    from datetime import datetime
    hoje = datetime.now().date()
    if d.date() == hoje:
        return t("Hoje", "Today", "今天", "Hoy")
    if (hoje - d.date()).days == 1:
        return t("Ontem", "Yesterday", "昨天", "Ayer")
    return f"{DIAS_SEMANA[d.weekday()]} {d:%m/%d}" if IDIOMA in ("en-US", "zh-CN") else f"{DIAS_SEMANA[d.weekday()]} {d:%d/%m}"


def historico(limite: int = 60) -> list[dict]:
    """Avisos mais recentes primeiro, com "detalhe" de quando ("Hoje 19:30", "Ontem 08:10", "Seg 05/10 14:00")."""
    import json
    from datetime import datetime
    with _trava:
        try:
            itens = json.loads(HISTORICO.read_text(encoding="utf-8")) if HISTORICO.exists() else []
        except (OSError, ValueError):
            itens = []
    saida = []
    for a in reversed(itens[-limite:]):
        q = datetime.fromtimestamp(a.get("quando", 0))
        saida.append({**a, "detalhe": f"{rotulo_dia(q)} {q:%H:%M}"})
    return saida


def avisos_desde(ultimo: int) -> dict:
    with _trava:
        # "atividade": sessões trabalhando agora (o Watcher mostra o Clawd trabalhando na tela de espera)
        # "sessoes": a lista do app Claude Code, para ele abrir sem esperar (o Watcher já consulta a cada 20 s)
        extra = {"atividade": dict(_atividade), "sessoes": list(_sessoes["lista"])}
        if ultimo < 0:  # primeira consulta do aparelho: só sincroniza, sem repetir avisos antigos
            return {"ultimo": _seq, "avisos": [], **extra}
        return {"ultimo": _seq, "avisos": [a for a in _avisos if a["id"] > ultimo], **extra}


def _frase_curta(titulo: str, fala: str) -> str:
    """Uma linha para a tela redonda: "Projeto: resultado" (até ~70 caracteres). Fallback: corta."""
    fala = (fala or "").strip()
    reserva = f"{titulo[:24]}: {fala[:44]}…" if fala else f"{titulo[:40]} " + t("terminou", "finished", "已完成", "terminó")
    if not fala:
        return reserva
    try:
        r = requests.post(f"{os.environ.get('API_BASE_URL', 'https://openrouter.ai/api/v1')}/chat/completions",
                          headers={"Authorization": f"Bearer {os.environ.get('API_KEY', '')}"}, timeout=12,
                          json={"model": os.environ.get("MODELO_LLM", "openai/gpt-6-luna"), "max_tokens": 400,
                                "reasoning": {"effort": "minimal"},
                                "messages": [{"role": "system", "content":
                                              f"Escreva UMA linha de no máximo 70 caracteres, em {NOME}, sem markdown, "
                                              "no formato 'Projeto curto: resultado'. Projeto curto = 1 a 3 palavras do título. "
                                              "Resultado = o que o agente concluiu, começando pelo resultado."},
                                             {"role": "user", "content": f"Título: {titulo}\nÚltima mensagem do agente: {fala[:1500]}"}]})
        r.raise_for_status()
        linha = (r.json()["choices"][0]["message"]["content"] or "").strip().strip('"')
        return linha[:80] if linha else reserva
    except Exception:
        return reserva


def _titulo_curto(a: dict) -> str:
    return a.get("titulo") or a.get("workspace") or a.get("pasta") or t("Sessão", "Session", "会话", "Sesión")


def _loop(ponte) -> None:
    estados: dict[str, str] = {}
    conclusoes: dict[str, int] = {}
    primeira = True
    while True:
        try:
            agentes = ponte._agentes()
            ativos = [_titulo_curto(a) for a in agentes
                      if a.get("situacao") in ("trabalhando", "subagentes rodando") or a.get("status") == "working"]
            compactas = sessoes_compactas(agentes)
            with _trava:
                _atividade.update(trabalhando=len(ativos), titulos=ativos[:3])
                _sessoes.update(quando=time.time(), lista=compactas)
            for a in agentes:
                chave, estado, feitas = a["sessao"], a["status"], a.get("_conclusoes", 0)
                if not primeira:
                    if estado == "blocked" and estados.get(chave) != "blocked":
                        adicionar_aviso("esperando", t("Esperando você", "Waiting for you", "等待你回复", "Esperándote"),
                                        f"{_titulo_curto(a)[:50]} " + t("precisa da sua resposta", "needs your answer",
                                                                        "需要你的回复", "necesita tu respuesta"), "warning",
                                        sessao=chave, nome_sessao=_titulo_curto(a))
                    # herdr soma completion_seq a cada tarefa terminada (pega até as que duram menos que o intervalo)
                    elif feitas > conclusoes.get(chave, feitas) and estado in ("idle", "done"):
                        time.sleep(3)  # dá tempo de o histórico registrar a fala final
                        info = ponte._historico_claude(a.get("_sessao_id", ""), a.get("_cwd", "")) if a.get("_sessao_id") else {}
                        fala = info.get("ultima_fala") or a.get("ultima_fala", "")
                        adicionar_aviso("concluiu", t("Tarefa concluída", "Task done", "任务完成", "Tarea completada"), _frase_curta(_titulo_curto(a), fala), "happy",
                                        sessao=a.get("sessao", ""), nome_sessao=_titulo_curto(a))
                estados[chave], conclusoes[chave] = estado, feitas
            primeira = False
        except Exception:
            pass
        time.sleep(INTERVALO_S)


_ponte = None


def ponte_carregada():
    """Módulo da ponte (mesma leitura do herdr e do histórico usada pelas ferramentas)."""
    global _ponte
    if _ponte is None:
        pasta_ponte = Path(__file__).resolve().parents[3] / "ponte-mcp"
        if str(pasta_ponte) not in sys.path:
            sys.path.insert(0, str(pasta_ponte))
        import ponte  # noqa: E402
        _ponte = ponte
    return _ponte


def iniciar_vigia() -> None:
    global _iniciado
    if _iniciado:
        return
    _iniciado = True
    threading.Thread(target=_loop, args=(ponte_carregada(),), daemon=True, name="vigia-sessoes").start()

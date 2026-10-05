"""Vigia das sessões: gera avisos para a tela do Watcher.

A cada 10 s olha o herdr (pela ponte) e detecta:
  - sessão passou a esperar você (permissão ou pergunta);
  - sessão terminou de trabalhar (com o resumo do que concluiu).
Outros módulos (ex.: reuniões) chamam adicionar_aviso. O Watcher busca em GET /watcher/avisos.
"""

import os
import sys
import threading
import time
from pathlib import Path

import requests

INTERVALO_S = 10
MAX_AVISOS = 50
_avisos: list[dict] = []
_seq = 0
_trava = threading.Lock()
_iniciado = False


def adicionar_aviso(tipo: str, titulo: str, texto: str, emocao: str = "neutral") -> None:
    global _seq
    with _trava:
        _seq += 1
        _avisos.append({"id": _seq, "tipo": tipo, "titulo": titulo[:40], "texto": texto[:110], "emocao": emocao,
                        "quando": int(time.time())})
        del _avisos[:-MAX_AVISOS]


def avisos_desde(ultimo: int) -> dict:
    with _trava:
        if ultimo < 0:  # primeira consulta do aparelho: só sincroniza, sem repetir avisos antigos
            return {"ultimo": _seq, "avisos": []}
        return {"ultimo": _seq, "avisos": [a for a in _avisos if a["id"] > ultimo]}


def _frase_curta(titulo: str, fala: str) -> str:
    """Uma linha para a tela redonda: "Projeto: resultado" (até ~70 caracteres). Fallback: corta."""
    fala = (fala or "").strip()
    reserva = f"{titulo[:24]}: {fala[:44]}…" if fala else f"{titulo[:40]} terminou"
    if not fala:
        return reserva
    try:
        r = requests.post(f"{os.environ.get('API_BASE_URL', 'https://openrouter.ai/api/v1')}/chat/completions",
                          headers={"Authorization": f"Bearer {os.environ.get('API_KEY', '')}"}, timeout=12,
                          json={"model": os.environ.get("MODELO_LLM", "openai/gpt-6-luna"), "max_tokens": 400,
                                "reasoning": {"effort": "minimal"},
                                "messages": [{"role": "system", "content":
                                              "Escreva UMA linha de no máximo 70 caracteres, português do Brasil, sem markdown, "
                                              "no formato 'Projeto curto: resultado'. Projeto curto = 1 a 3 palavras do título. "
                                              "Resultado = o que o agente concluiu, começando pelo resultado."},
                                             {"role": "user", "content": f"Título: {titulo}\nÚltima mensagem do agente: {fala[:1500]}"}]})
        r.raise_for_status()
        linha = (r.json()["choices"][0]["message"]["content"] or "").strip().strip('"')
        return linha[:80] if linha else reserva
    except Exception:
        return reserva


def _titulo_curto(a: dict) -> str:
    return a.get("titulo") or a.get("workspace") or a.get("pasta") or "Sessão"


def _loop(ponte) -> None:
    estados: dict[str, str] = {}
    conclusoes: dict[str, int] = {}
    primeira = True
    while True:
        try:
            for a in ponte._agentes():
                chave, estado, feitas = a["sessao"], a["status"], a.get("_conclusoes", 0)
                if not primeira:
                    if estado == "blocked" and estados.get(chave) != "blocked":
                        adicionar_aviso("espera", "Esperando você", f"{_titulo_curto(a)[:50]} precisa da sua resposta", "warning")
                    # herdr soma completion_seq a cada tarefa terminada (pega até as que duram menos que o intervalo)
                    elif feitas > conclusoes.get(chave, feitas) and estado in ("idle", "done"):
                        time.sleep(3)  # dá tempo de o histórico registrar a fala final
                        info = ponte._historico_claude(a.get("_sessao_id", ""), a.get("_cwd", "")) if a.get("_sessao_id") else {}
                        fala = info.get("ultima_fala") or a.get("ultima_fala", "")
                        adicionar_aviso("concluiu", "Tarefa concluída", _frase_curta(_titulo_curto(a), fala), "happy")
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

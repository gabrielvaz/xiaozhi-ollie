"""Previsão do tempo para onde o Watcher está (ou para a cidade pedida).

Local: IP público do aparelho (x-forwarded-for do Funnel) -> ip-api.com.
Previsão: Open-Meteo (sem chave). Copiado para plugins_func/functions pelo iniciar.py.
"""

import requests

from config.logger import setup_logging
from plugins_func.register import Action, ActionResponse, ToolType, register_function

TAG = __name__
logger = setup_logging()

PREVISAO_TEMPO_DESC = {
    "type": "function",
    "function": {
        "name": "previsao_tempo",
        "description": (
            "Previsão do tempo de hoje e amanhã. Sem cidade, usa a localização aproximada do Watcher "
            "(pela conexão de internet). Use quando perguntarem do tempo, chuva, temperatura ou se precisa de guarda-chuva."
        ),
        "parameters": {
            "type": "object",
            "properties": {
                "cidade": {"type": "string", "description": "Cidade específica, se o usuário citar uma"},
            },
            "required": [],
        },
    },
}

CODIGOS = {
    0: "céu limpo", 1: "quase limpo", 2: "parcialmente nublado", 3: "nublado", 45: "neblina", 48: "neblina com geada",
    51: "garoa fraca", 53: "garoa", 55: "garoa forte", 61: "chuva fraca", 63: "chuva", 65: "chuva forte",
    66: "chuva congelante", 67: "chuva congelante forte", 71: "neve fraca", 73: "neve", 75: "neve forte",
    80: "pancadas de chuva fracas", 81: "pancadas de chuva", 82: "pancadas de chuva fortes",
    95: "trovoadas", 96: "trovoadas com granizo", 99: "trovoadas fortes com granizo",
}


def _local_por_ip(ip):
    # IP local/privado (teste no próprio Mac): ip-api sem IP usa o IP público de quem pergunta, ou seja, o de casa
    privado = not ip or ip.startswith(("127.", "10.", "192.168.", "100.", "::1")) or ip.startswith("172.")
    r = requests.get(f"http://ip-api.com/json/{'' if privado else ip}", params={"lang": "pt-BR", "fields": "status,city,regionName,lat,lon"}, timeout=5).json()
    if r.get("status") != "success":
        return None
    return {"nome": f"{r['city']}, {r['regionName']}", "lat": r["lat"], "lon": r["lon"]}


def _local_por_nome(cidade):
    r = requests.get("https://geocoding-api.open-meteo.com/v1/search",
                     params={"name": cidade, "count": 1, "language": "pt", "format": "json"}, timeout=5).json()
    if not r.get("results"):
        return None
    c = r["results"][0]
    return {"nome": f"{c['name']}, {c.get('admin1', c.get('country', ''))}", "lat": c["latitude"], "lon": c["longitude"]}


def categoria(codigo: int) -> str:
    """Categoria do ícone desenhado no Watcher."""
    if codigo in (0, 1):
        return "sol"
    if codigo == 2:
        return "parcial"
    if codigo == 3:
        return "nuvem"
    if codigo in (45, 48):
        return "neblina"
    if codigo in (51, 53, 55):
        return "garoa"
    if codigo in (71, 73, 75):
        return "neve"
    if codigo in (95, 96, 99):
        return "tempestade"
    return "chuva"


def dados_tempo(ip: str | None, cidade: str | None = None) -> dict:
    """Previsão estruturada (sem IA) para a tela do Watcher."""
    local = _local_por_nome(cidade) if cidade else _local_por_ip(ip)
    if not local:
        return {"ok": False, "erro": "Não consegui descobrir a localização."}
    p = requests.get("https://api.open-meteo.com/v1/forecast", params={
        "latitude": local["lat"], "longitude": local["lon"], "timezone": "America/Sao_Paulo", "forecast_days": 2,
        "current": "temperature_2m,apparent_temperature,weather_code,relative_humidity_2m,is_day",
        "daily": "temperature_2m_max,temperature_2m_min,precipitation_probability_max,weather_code",
    }, timeout=8).json()
    a, d = p["current"], p["daily"]
    dias = [{"nome": nome, "categoria": categoria(d["weather_code"][i]),
             "descricao": CODIGOS.get(d["weather_code"][i], "tempo variável"),
             "min": round(d["temperature_2m_min"][i]), "max": round(d["temperature_2m_max"][i]),
             "chuva": d["precipitation_probability_max"][i] or 0} for i, nome in enumerate(("Hoje", "Amanhã"))]
    return {"ok": True, "local": local["nome"].split(",")[0], "temp": round(a["temperature_2m"]),
            "sensacao": round(a["apparent_temperature"]), "umidade": a["relative_humidity_2m"],
            "categoria": categoria(a["weather_code"]), "noite": not a.get("is_day", 1),
            "descricao": CODIGOS.get(a["weather_code"], "tempo variável"), "dias": dias}


@register_function("previsao_tempo", PREVISAO_TEMPO_DESC, ToolType.SYSTEM_CTL)
def previsao_tempo(conn, cidade: str = None):
    try:
        local = _local_por_nome(cidade) if cidade else _local_por_ip(conn.client_ip)
        if not local:
            return ActionResponse(Action.REQLLM, "Não consegui descobrir a localização. Pergunte a cidade ao usuário.", None)
        p = requests.get("https://api.open-meteo.com/v1/forecast", params={
            "latitude": local["lat"], "longitude": local["lon"], "timezone": "America/Sao_Paulo", "forecast_days": 2,
            "current": "temperature_2m,apparent_temperature,weather_code,relative_humidity_2m",
            "daily": "temperature_2m_max,temperature_2m_min,precipitation_probability_max,weather_code",
        }, timeout=8).json()
        a, d = p["current"], p["daily"]
        dias = []
        for i, nome in enumerate(("hoje", "amanhã")):
            dias.append(f"{nome}: {CODIGOS.get(d['weather_code'][i], 'tempo variável')}, mínima {round(d['temperature_2m_min'][i])}°, "
                        f"máxima {round(d['temperature_2m_max'][i])}°, chance de chuva {d['precipitation_probability_max'][i]}%")
        texto = (f"Local: {local['nome']}{' (aproximado pela conexão)' if not cidade else ''}. "
                 f"Agora: {round(a['temperature_2m'])}°, sensação {round(a['apparent_temperature'])}°, "
                 f"{CODIGOS.get(a['weather_code'], 'tempo variável')}, umidade {a['relative_humidity_2m']}%. "
                 + "; ".join(dias) + ". Responda em uma ou duas frases faladas.")
        return ActionResponse(Action.REQLLM, texto, None)
    except Exception as e:
        logger.bind(tag=TAG).error(f"previsao_tempo: {e}")
        return ActionResponse(Action.REQLLM, "O serviço de previsão do tempo não respondeu agora.", None)

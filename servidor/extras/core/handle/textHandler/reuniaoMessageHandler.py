"""Mensagem {"type": "reuniao", "state": "start" | "pause" | "resume" | "stop"} enviada pela gaveta de ações do Watcher.

Respostas: start/resume -> {"state": "recording"}; pause -> {"state": "paused"};
stop -> {"state": "saved", "duration": "12 min"}.
"""

import json
import time
from typing import Any, Dict

from core.handle.textMessageHandler import TextMessageHandler
from core.handle.textMessageType import TextMessageType
from core.utils.reuniao import GravadorReuniao

TAG = __name__


class ReuniaoTextMessageHandler(TextMessageHandler):
    @property
    def message_type(self) -> TextMessageType:
        return TextMessageType.REUNIAO

    async def handle(self, conn, msg_json: Dict[str, Any]) -> None:
        estado = msg_json.get("state")
        if estado == "start":
            if getattr(conn, "reuniao", None) is None:
                conn.reuniao = GravadorReuniao(conn.logger.bind(tag=TAG))
                conn.logger.bind(tag=TAG).info(f"Reunião iniciada em {conn.reuniao.pasta}")
            await conn.websocket.send(json.dumps({"type": "reuniao", "state": "recording"}))
        elif estado in ("pause", "resume"):
            gravador = getattr(conn, "reuniao", None)
            if gravador is not None:
                gravador.pausar() if estado == "pause" else gravador.continuar()
                conn.logger.bind(tag=TAG).info(f"Reunião {'pausada' if estado == 'pause' else 'retomada'}")
            pausado = gravador is not None and gravador.pausado
            await conn.websocket.send(json.dumps({"type": "reuniao", "state": "paused" if pausado else "recording"}))
        elif estado == "stop":
            gravador = getattr(conn, "reuniao", None)
            conn.reuniao = None
            duracao = gravador.parar() if gravador else ""
            conn.logger.bind(tag=TAG).info(f"Reunião encerrada ({duracao}); processando em segundo plano")
            await conn.websocket.send(json.dumps({"type": "reuniao", "state": "saved", "duration": duracao}))
        conn.last_activity_time = time.time() * 1000

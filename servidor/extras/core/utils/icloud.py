"""Leitura resistente de arquivos do iCloud Drive.

Enquanto o iCloud sincroniza um arquivo, processos comuns (como o serviço do launchd) recebem
"Resource deadlock avoided" (errno 11) ao abri-lo. Tentar de novo após uma pausa curta resolve.
"""

import errno
import time
from pathlib import Path


def ler_texto(caminho, tentativas: int = 8, pausa_s: float = 0.4, padrao: str | None = None) -> str:
    caminho = Path(caminho)
    for i in range(tentativas):
        try:
            return caminho.read_text(encoding="utf-8")
        except OSError as e:
            if e.errno != errno.EDEADLK or i == tentativas - 1:
                if padrao is not None:
                    return padrao
                raise
            time.sleep(pausa_s)
    return padrao or ""

"""Traduz para pt-BR as frases do xiaozhi-server que chegam à tela ou à voz do Watcher.

Idempotente: pode rodar de novo depois de atualizar o servidor. Cada troca
precisa achar o texto original (ou já estar traduzida); senão, avisa.
Uso: python patches/traduzir_servidor.py [pasta-do-xiaozhi-server]
"""

import sys
from pathlib import Path

RAIZ = Path(sys.argv[1] if len(sys.argv) > 1 else Path(__file__).resolve().parents[1] / "xiaozhi-server")

TROCAS: list[tuple[str, str, str]] = [
    # Respostas rápidas à palavra de ativação ("Jarvis")
    ("core/handle/helloHandle.py", '''        "我一直都在呢，您请说。",
        "在的呢，请随时吩咐我。",
        "来啦来啦，请告诉我吧。",
        "您请说，我正听着。",
        "请您讲话，我准备好了。",
        "请您说出指令吧。",
        "我认真听着呢，请讲。",
        "请问您需要什么帮助？",
        "我在这里，等候您的指令。",''', '''        "Pois não?",
        "Estou aqui, pode falar.",
        "Diga.",
        "Às ordens.",
        "Pode mandar.",
        "Estou ouvindo.",'''),
    # Sem resposta pronta ainda: em vez de tocar o áudio padrão em chinês, segue o fluxo normal e gera a resposta em pt-BR
    ("core/handle/helloHandle.py", '''    conn.just_woken_up = True
    await send_tts_message(conn, "start")

    # 获取当前音色
    voice = getattr(conn.tts, "voice", "default")
    if not voice:
        voice = "default"

    # 获取唤醒词回复配置
    response = wakeup_words_config.get_wakeup_response(voice)
    if not response or not response.get("file_path"):
        response = {
            "voice": "default",
            "file_path": "config/assets/wakeup_words_short.wav",
            "time": 0,
            "text": "我在这里哦！",
        }''', '''    # Voz atual e resposta pronta para a ativação (gerada em pt-BR pelo TTS)
    voice = getattr(conn.tts, "voice", "default")
    if not voice:
        voice = "default"
    response = wakeup_words_config.get_wakeup_response(voice)
    if not response or not response.get("file_path"):
        # Ainda não há resposta em pt-BR: gera em segundo plano e segue o fluxo normal
        if not _wakeup_response_lock.locked():
            asyncio.create_task(wakeupWordsResponse(conn))
        return False

    conn.just_woken_up = True
    await send_tts_message(conn, "start")'''),
    ("core/handle/intentHandler.py", "工具调用超时，请一会再试下哈", "A ferramenta demorou demais. Tenta de novo daqui a pouco."),
    ("core/connection.py", "当用户的请求不匹配其他任何工具时，可用此选项直接回复。将回复内容写在response参数里。",
     "Quando o pedido do usuário não combina com nenhuma outra ferramenta, use esta para responder direto. Escreva a resposta no parâmetro response."),
    ("core/connection.py", '"服务器重启中..."', '"Servidor reiniciando..."'),
    ("core/connection.py", 'content="给我讲个故事吧"', 'content="Me conta uma curiosidade"'),
    ("core/connection.py", '好呀，你想听什么类型的呀？童话、冒险还是搞笑的？选一个我给你开讲~', 'Claro. Prefere ciência, história ou tecnologia?'),
    ("core/connection.py", 'content="已直接回复"', 'content="Respondido diretamente"'),
    ("core/connection.py", 'content="拜拜"', 'content="Tchau"'),
    ("core/connection.py", '再见，下次再聊~', 'Tchau, até a próxima.'),
    ("core/connection.py", 'content="退出意图已处理"', 'content="Despedida processada"'),
    ("core/connection.py", "[系统提示] 已达到最大工具调用次数限制，请你基于目前已经获取的所有信息，直接给出最终答案。不要再尝试调用任何工具。",
     "[Aviso do sistema] O limite de chamadas de ferramentas foi atingido. Responda agora com o que já tem, sem chamar mais ferramentas."),
    ("core/connection.py", "哎呀，网络遇到点问题，请稍后再试下！", "Ops, a rede falhou. Tenta de novo daqui a pouco."),
    ("core/handle/receiveAudioHandle.py", "不好意思，我现在有点事情要忙，明天这个时候我们再聊，约好了哦！明天不见不散，拜拜！",
     "Por hoje chega de conversa, atingi o limite de uso. Amanhã a gente continua. Tchau!"),
    ("core/handle/receiveAudioHandle.py", "没有找到该设备的版本信息，请正确配置 OTA地址，然后重新编译固件。",
     "Não encontrei a versão deste aparelho. Confira o endereço OTA e grave o firmware de novo."),
    ("core/providers/tools/unified_tool_handler.py", '"无法解析函数参数"', '"Não consegui entender os parâmetros da ferramenta."'),
    ("core/providers/tools/unified_tool_handler.py", '"无响应"', '"Sem resposta."'),
    ("core/providers/tools/unified_tool_manager.py", 'f"工具 {tool_name} 不存在"', 'f"A ferramenta {tool_name} não existe."'),
    ("core/providers/tools/server_mcp/mcp_executor.py", '"MCP管理器未初始化"', '"A ponte com o Mac ainda não está pronta."'),
    ("core/providers/tools/device_mcp/mcp_executor.py", '"设备端MCP客户端未初始化"', '"As ferramentas do aparelho ainda não estão prontas."'),
    ("core/providers/tools/device_mcp/mcp_executor.py", '"设备端MCP客户端未准备就绪"', '"As ferramentas do aparelho ainda não estão prontas."'),
    # Correção: o streaming da resposta apagava o espaço no fim de cada pedaço ("Como " + "posso" = "Comoposso")
    ("core/connection.py", '''        result = re.sub(r'["\\'}\\]]+$', '', result.rstrip()).rstrip()
        return result''', '''        lixo = re.search(r'["\\'}\\]]+\\s*$', result)
        if lixo:
            result = result[: lixo.start()].rstrip()
        return result'''),
    # Ferramenta chinesa de calendário lunar carregada por padrão: não serve aqui
    ("core/providers/tools/server_plugins/plugin_executor.py", 'necessary_functions = ["handle_exit_intent", "get_lunar"]', 'necessary_functions = ["handle_exit_intent"]'),
    # Ao dizer "Jarvis", o servidor mandava "ei, olá" em chinês ao modelo (e à tela)
    ("core/handle/textHandler/listenMessageHandler.py", '"嘿，你好呀"',
     'f"Oi, {__import__(\'core.utils.perfil\', fromlist=[\'x\']).nome_agente()}"'),
    # Correção: comando enviado logo ao conectar (3 cliques) chegava antes das ferramentas da ponte carregarem
    ("core/connection.py", """            functions = list(self.func_handler.get_functions())
            # 仅在第一层调用时注入""", """            espera = 0
            while not getattr(self.func_handler, "finish_init", True) and espera < 50:
                time.sleep(0.1)  # espera até 5 s pelas ferramentas da ponte
                espera += 1
            functions = list(self.func_handler.get_functions())
            # 仅在第一层调用时注入"""),
    # Correção: fala logo ao conectar chegava antes do prompt e das ferramentas (o modelo respondia sem nada)
    ("core/connection.py", """        # Define intent functions
        functions = None""", """        espera = 0
        while depth == 0 and espera < 80 and (not hasattr(self, "func_handler") or
                                              not any(m.role == "system" for m in self.dialogue.dialogue)):
            time.sleep(0.1)  # até 8 s pelo prompt e pelas ferramentas
            espera += 1
        # Define intent functions
        functions = None"""),
    # Nome do agente escolhido no Watcher (extras/core/utils/perfil.py) troca "Ollie" no prompt
    ("core/utils/prompt_manager.py", """                base_prompt=user_prompt,""",
     """                base_prompt=__import__("core.utils.perfil", fromlist=["aplicar"]).aplicar(user_prompt),"""),
    ("core/utils/prompt_manager.py", '''        """快速获取系统提示词（使用用户配置）"""
''', '''        """快速获取系统提示词（使用用户配置）"""
        user_prompt = __import__("core.utils.perfil", fromlist=["aplicar"]).aplicar(user_prompt)
'''),
    # Registro de todas as conversas (extras/core/utils/diario.py)
    ("core/utils/dialogue.py", """    def put(self, message: Message):
        self.dialogue.append(message)""", """    def put(self, message: Message):
        self.dialogue.append(message)
        from core.utils.diario import registrar
        registrar(message, self)"""),
    # Gravador de reuniões (extras/core/utils/reuniao.py e reuniaoMessageHandler.py)
    ("core/handle/textMessageType.py", '    PING = "ping"', '    PING = "ping"\n    REUNIAO = "reuniao"'),
    ("core/handle/textMessageHandlerRegistry.py", "from core.handle.textHandler.pingMessageHandler import PingMessageHandler",
     "from core.handle.textHandler.pingMessageHandler import PingMessageHandler\nfrom core.handle.textHandler.reuniaoMessageHandler import ReuniaoTextMessageHandler"),
    ("core/handle/textMessageHandlerRegistry.py", "            PingMessageHandler(),\n", "            PingMessageHandler(),\n            ReuniaoTextMessageHandler(),\n"),
    ("core/handle/receiveAudioHandle.py", """async def handleAudioMessage(conn: "ConnectionHandler", pcm_frame):
""", """async def handleAudioMessage(conn: "ConnectionHandler", pcm_frame):
    # Reunião em gravação: o áudio vai para o arquivo, sem VAD, ASR ou resposta
    if getattr(conn, "reuniao", None) is not None:
        conn.reuniao.adicionar(pcm_frame)
        conn.last_activity_time = time.time() * 1000
        return
"""),
    ("core/connection.py", """            # 清理opus解码器
            if hasattr(self, "_connection_opus_decoder"):""", """            # Conexão caiu no meio de uma reunião: salva e processa o que foi gravado
            if getattr(self, "reuniao", None) is not None:
                try:
                    self.reuniao.parar()
                except Exception:
                    pass
                self.reuniao = None

            # 清理opus解码器
            if hasattr(self, "_connection_opus_decoder"):"""),
    # Câmera: o servidor mandava responder em chinês
    ("core/providers/vllm/openai.py", 'question = question + "(请使用中文回复)"', 'question = question + " (Responda em português do Brasil, em até 3 frases curtas.)"'),
    # Tela: frase amigável no lugar de "% nome_da_ferramenta" (extras/core/utils/avisos.py)
    ("core/providers/tools/unified_tool_handler.py", 'await send_display_message(self.conn, f"% {function_name}")',
     'from core.utils.avisos import aviso_ferramenta\n                if aviso_ferramenta(function_name):\n                    await send_display_message(self.conn, aviso_ferramenta(function_name))'),
    # Rotas do Watcher (avisos, sessões, uso do Claude, microSD, memória) e vigia das sessões — um único patch
    ("core/http_server.py", "from core.api.vision_handler import VisionHandler",
     "from core.api.vision_handler import VisionHandler\nfrom core.api.avisos_handler import AvisosHandler\n"
     "from core.api.cartao_handler import CartaoHandler\nfrom core.api.conversas_handler import ConversasHandler\n"
     "from core.api.reunioes_handler import ReunioesHandler\n"
     "from core.utils.vigia import iniciar_vigia"),
    ("core/http_server.py", """                # 添加路由
                app.add_routes(""", """                avisos = AvisosHandler(self.config)
                cartao = CartaoHandler(self.config)
                conversas = ConversasHandler(self.config)
                reunioes = ReunioesHandler(self.config)
                app.add_routes([web.get("/watcher/avisos", avisos.handle_get),
                                web.get("/watcher/sessoes", avisos.handle_sessoes),
                                web.get("/watcher/sessoes/{id}/mensagens", avisos.handle_mensagens),
                                web.get("/watcher/uso", avisos.handle_uso),
                                web.get("/watcher/qrcodes", avisos.handle_qrcodes),
                                web.get("/watcher/tempo", avisos.handle_tempo),
                                web.post("/watcher/upload", cartao.handle_upload),
                                web.get("/watcher/memoria", cartao.handle_memoria),
                                web.get("/watcher/memoria/audio/{nome}", cartao.handle_audio),
                                web.get("/watcher/perfil", conversas.handle_perfil),
                                web.get("/watcher/conversas", conversas.handle_lista),
                                web.get("/watcher/conversas/{id}", conversas.handle_conversa),
                                web.get("/watcher/conversas/{id}/audio", conversas.handle_audio),
                                web.get("/watcher/reunioes", reunioes.handle_lista)])
                iniciar_vigia()
                # 添加路由
                app.add_routes("""),
    # Depuração: WATCHER_REGISTRAR_PROMPT=1 grava a última requisição ao modelo em data/ultimo_prompt.json
    ("core/providers/llm/openai/openai.py", """        stream = self.client.chat.completions.create(**request_params)""", """        if os.environ.get("WATCHER_REGISTRAR_PROMPT") == "1":
            import json as _json
            with open("data/ultimo_prompt.json", "w", encoding="utf-8") as _f:
                _json.dump(request_params, _f, ensure_ascii=False, indent=1, default=str)
        stream = self.client.chat.completions.create(**request_params)"""),
    ("core/providers/llm/openai/openai.py", """        responses = self.client.chat.completions.create(**request_params)""", """        if os.environ.get("WATCHER_REGISTRAR_PROMPT") == "1":
            import json as _json
            with open("data/ultimo_prompt_sem_ferramentas.json", "w", encoding="utf-8") as _f:
                _json.dump(request_params, _f, ensure_ascii=False, indent=1, default=str)
        responses = self.client.chat.completions.create(**request_params)"""),
    # Prompt: dia da semana em português e bloco <memoria> com iCloud Drive/Watcher/Memória/*.md
    ("core/utils/prompt_manager.py", """WEEKDAY_MAP = {
    "Monday": "星期一",
    "Tuesday": "星期二",
    "Wednesday": "星期三",
    "Thursday": "星期四",
    "Friday": "星期五",
    "Saturday": "星期六",
    "Sunday": "星期日",""", """WEEKDAY_MAP = {
    "Monday": "segunda-feira",
    "Tuesday": "terça-feira",
    "Wednesday": "quarta-feira",
    "Thursday": "quinta-feira",
    "Friday": "sexta-feira",
    "Saturday": "sábado",
    "Sunday": "domingo","""),
    ("core/utils/prompt_manager.py", """                dynamic_context=self.context_data,
                language=language,""", """                dynamic_context=self.context_data,
                language=language,
                memoria=__import__("core.utils.memoria_usuario", fromlist=["ler"]).ler(),"""),
    ("plugins_func/functions/handle_exit_intent.py", '"当用户想结束对话或需要退出系统时调用"',
     '"Chame quando o usuário quiser encerrar a conversa ou se despedir (tchau, até mais, boa noite, pode dormir)."'),
    ("plugins_func/functions/handle_exit_intent.py", '"和用户友好结束对话的告别语"', '"Despedida curta e simpática em português"'),
    # O prompt rápido (1º turno após reiniciar) também leva a memória do usuário
    ("core/utils/prompt_manager.py", """        # 使用传入的提示词并缓存（如果有设备ID）
        if device_id:""", """        # prompt rápido também leva a memória (sobre-mim.md e lembretes)
        memoria = __import__("core.utils.memoria_usuario", fromlist=["ler"]).ler()
        if memoria:
            quebra = chr(10)
            user_prompt = user_prompt + quebra * 2 + "<memoria>" + quebra + memoria + quebra + "</memoria>"
        # 使用传入的提示词并缓存（如果有设备ID）
        if device_id:"""),
    ("plugins_func/functions/handle_exit_intent.py", 'result="退出意图已处理"', 'result="Despedida processada"'),
    ("plugins_func/functions/handle_exit_intent.py", 'result="退出意图处理失败"', 'result="Falha ao processar a despedida"'),
]


def main() -> int:
    problemas = 0
    for rel, antigo, novo in TROCAS:
        arq = RAIZ / rel
        texto = arq.read_text(encoding="utf-8")
        if novo in texto:
            print(f"já estava  {rel}: {novo.strip().splitlines()[0][:60]}")
            continue
        if antigo in texto:
            arq.write_text(texto.replace(antigo, novo), encoding="utf-8")
            print(f"traduzido  {rel}: {novo.strip().splitlines()[0][:60]}")
        elif novo in texto:
            print(f"já estava  {rel}: {novo.strip().splitlines()[0][:60]}")
        else:
            problemas += 1
            print(f"NÃO ACHEI  {rel}: {antigo.strip().splitlines()[0][:60]}")
    return 1 if problemas else 0


if __name__ == "__main__":
    sys.exit(main())

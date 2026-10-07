// App "Claude Code": lista as sessões do Claude Code/Codex no herdr, mostra as últimas mensagens (roláveis),
// manda um pedido por voz e responde pela tela às perguntas da sessão (escolha única ou múltipla,
// e pedidos de permissão), ou pede ao agente para ler as opções e responder por voz.
// Aberto por um aviso (sessão esperando você ou tarefa concluída), mostra o aviso com o botão "Continuar".
#pragma once

#include <atomic>
#include <cctype>
#include <vector>

#include "../agente_watcher.h"
#include "../idioma_watcher.h"
#include "../nucleo_apps.h"

class AppSessoes : public AppWatcher {
public:
    const char* Nome() const override { return "Claude Code"; }
    const char* Id() const override { return "sessoes"; }
    const char* Icone() const override { return PainelWatcher::kIconeClaude; }  // logo desenhado
    std::string Detalhe() const override { return TR("Ver, ler e mandar pedidos", "View, read and send requests", "查看、阅读和发送请求", "Ver, leer y enviar peticiones"); }

    void Abrir(ContextoApps& c) override {
        ir_para_.clear();
        ir_titulo_.clear();
        tela_ = Tela::Carregando;
        c.painel.MostrarStatus("Claude Code", PainelWatcher::Status::Carregando,
                               TR("Carregando sessões…", "Loading sessions…", "正在加载会话…", "Cargando sesiones…"));
        pedido_ = Pedido::Lista;
    }

    // Argumento "aviso\n<id>\n<título>\n<texto>": tela do aviso com "Continuar" (abre a sessão)
    void AbrirCom(ContextoApps& c, const std::string& argumento) override {
        std::vector<std::string> partes;
        size_t ini = 0;
        while (partes.size() < 3) {
            size_t fim = argumento.find('\n', ini);
            if (fim == std::string::npos) {
                break;
            }
            partes.push_back(argumento.substr(ini, fim - ini));
            ini = fim + 1;
        }
        if (argumento.rfind("ir\n", 0) == 0) {  // "ir\n<id>[\n<título>]": abre direto a sessão (ex.: de um aviso)
            std::string resto = argumento.substr(3);
            auto quebra = resto.find('\n');
            ir_para_ = resto.substr(0, quebra);
            ir_titulo_ = quebra == std::string::npos ? "" : resto.substr(quebra + 1);
            tela_ = Tela::Carregando;
            c.painel.MostrarStatus("Claude Code", PainelWatcher::Status::Carregando, TR("Abrindo a sessão…", "Opening session…", "正在打开会话…", "Abriendo la sesión…"), {}, "reading");
            pedido_ = Pedido::Lista;
            return;
        }
        if (partes.size() < 3 || partes[0] != "aviso") {
            Abrir(c);
            return;
        }
        ir_para_ = partes[1];
        tela_ = Tela::Aviso;
        c.painel.MostrarTexto(partes[2], argumento.substr(ini), {TR("Continuar", "Continue", "继续", "Continuar"), TR("Fechar", "Close", "关闭", "Cerrar")});
    }

    void Girar(ContextoApps& c, int passo) override {
        if (tela_ != Tela::Carregando && tela_ != Tela::Erro) {
            c.painel.Mover(passo);
        }
    }

    bool Voltar(ContextoApps& c) override {
        if (tela_ == Tela::Pergunta || tela_ == Tela::Resultado) {
            MostrarDetalhe(c);
            return true;
        }
        if (tela_ == Tela::Detalhe || tela_ == Tela::NaoAchei) {
            MostrarLista(c, tela_ == Tela::Detalhe ? atual_ : 0);
            return true;
        }
        return false;
    }

    bool Clicar(ContextoApps& c) override {
        int i = c.painel.Selecionado();
        switch (tela_) {
            case Tela::Carregando:
                return true;
            case Tela::Erro:
                return false;
            case Tela::Aviso:
                if (i == 0) {  // Ir para a sessão: carrega a lista e abre a sessão do aviso
                    tela_ = Tela::Carregando;
                    c.painel.MostrarStatus("Claude Code", PainelWatcher::Status::Carregando, TR("Abrindo a sessão…", "Opening session…", "正在打开会话…", "Abriendo la sesión…"), {}, "reading");
                    pedido_ = Pedido::Lista;
                    return true;
                }
                c.fechar();
                return true;
            case Tela::Lista:
                if (i == 1) {  // resumo falado de todas as sessões ativas
                    c.Perguntar(TR("Me dá uma atualização de todas as sessões ativas do Claude Code e do herdr: quantas estão "
                                   "trabalhando, quais esperam aprovação, quais têm subagentes e o que cada uma concluiu ou está fazendo.",
                                   "Give me an update on all active Claude Code and herdr sessions: how many are working, "
                                   "which are waiting for approval, which have subagents, and what each one finished or is doing.",
                                   "给我汇报一下 Claude Code 和 herdr 的所有活跃会话：有几个正在工作，哪些在等待批准，"
                                   "哪些有子代理，以及每个会话完成了什么或正在做什么。",
                                   "Dame un resumen de todas las sesiones activas de Claude Code y de herdr: cuántas están "
                                   "trabajando, cuáles esperan aprobación, cuáles tienen subagentes y qué ha terminado o está haciendo cada una."));
                    return true;
                }
                if (i == 2) {  // sessão nova por voz
                    c.Perguntar(TR("Quero começar uma sessão nova do Claude Code. Pergunte em qual projeto e o que devo pedir; "
                                   "depois confirme e use claude_nova_sessao.",
                                   "I want to start a new Claude Code session. Ask which project and what to request; "
                                   "then confirm and use claude_nova_sessao.",
                                   "我想新建一个 Claude Code 会话。问我用哪个项目、要做什么；确认后使用 claude_nova_sessao。",
                                   "Quiero empezar una sesión nueva de Claude Code. Pregunta en qué proyecto y qué pedir; "
                                   "luego confirma y usa claude_nova_sessao."));
                    return true;
                }
                if (i <= 0 || i > (int)sessoes_.size() + 2) {  // item 0 = Voltar
                    return false;
                }
                AbrirSessao(c, i - 3);
                return true;
            case Tela::Detalhe: {
                // Botões: [Responder,] Enviar pedido, Voltar
                int b = pergunta_.texto.empty() ? i + 1 : i;
                if (b == 0) {
                    MostrarPergunta(c, 2);
                } else if (b == 1) {
                    const auto& s = sessoes_[atual_];
                    c.Perguntar(TR("Quero mandar um pedido para a sessão \"", "I want to send a request to the session \"",
                                   "我想给会话\"", "Quiero enviar una petición a la sesión \"") +
                                s.titulo + "\" (id " + s.id +
                                TR("). Pergunte o que devo enviar e, depois que eu responder, confirme e use sessao_instruir nessa sessão.",
                                   "). Ask me what to send and, after I answer, confirm and use sessao_instruir on that session.",
                                   "）发送一个请求。问我要发送什么，我回答后先确认，再对该会话使用 sessao_instruir。",
                                   "). Pregúntame qué debo enviar y, cuando responda, confirma y usa sessao_instruir en esa sesión."));
                } else {
                    MostrarLista(c, atual_);
                }
                return true;
            }
            case Tela::Pergunta: {
                int n = (int)pergunta_.opcoes.size();
                if (i == 0) {  // Voltar
                    MostrarDetalhe(c);
                } else if (i == 1) {  // o agente lê as opções e responde por voz
                    const auto& s = sessoes_[atual_];
                    c.Perguntar(TR("A sessão \"", "The session \"", "会话\"", "La sesión \"") + s.titulo + "\" (id " + s.id +
                                TR(") está me fazendo uma pergunta. Use "
                                   "sessao_pergunta nessa sessão, leia a pergunta e as opções numeradas para mim, espere eu "
                                   "responder, confirme a escolha e use sessao_escolher.",
                                   ") is asking me a question. Use sessao_pergunta on that session, read me the question and "
                                   "the numbered options, wait for my answer, confirm the choice and use sessao_escolher.",
                                   "）在问我一个问题。请对该会话使用 sessao_pergunta，把问题和编号选项读给我听，等我回答，"
                                   "确认选择后使用 sessao_escolher。",
                                   ") me está haciendo una pregunta. Usa sessao_pergunta en esa sesión, léeme la pregunta y "
                                   "las opciones numeradas, espera mi respuesta, confirma la elección y usa sessao_escolher."));
                } else if (i >= 2 && i < 2 + n) {
                    auto& op = pergunta_.opcoes[i - 2];
                    if (pergunta_.multipla) {
                        op.marcada = !op.marcada;
                        MostrarPergunta(c, i);
                    } else {
                        Responder(c, {op.n});
                    }
                } else if (pergunta_.multipla && i == 2 + n) {  // Enviar
                    std::vector<int> escolhas;
                    for (const auto& op : pergunta_.opcoes) {
                        if (op.marcada) {
                            escolhas.push_back(op.n);
                        }
                    }
                    if (!escolhas.empty()) {
                        Responder(c, escolhas);
                    }
                }
                return true;
            }
            case Tela::Resultado:
                MostrarDetalhe(c);
                return true;
            case Tela::NaoAchei:
                if (i == 0) {
                    MostrarLista(c, 0);
                } else {
                    c.fechar();
                }
                return true;
        }
        return false;
    }

    void Tique(ContextoApps& c) override {
        Pedido p = pedido_.exchange(Pedido::Nada);
        if (p == Pedido::Lista) {
            BuscarLista(c);
        } else if (p == Pedido::Sessao) {
            LerSessao(c);
        } else if (p == Pedido::Responder) {
            EnviarResposta(c);
        } else if (refrescar_) {
            // A lista veio do cache: busca a fresca e redesenha sem piscar, mantendo a seleção
            refrescar_ = false;
            std::string corpo;
            if (tela_ != Tela::Lista || !RedeWatcher::Pedir("GET", "/watcher/sessoes", "", corpo)) {
                return;
            }
            std::vector<Sessao> novas;
            cJSON* raiz = cJSON_Parse(corpo.c_str());
            cJSON* lista = raiz ? cJSON_GetObjectItem(raiz, "sessoes") : nullptr;
            cJSON* item = nullptr;
            cJSON_ArrayForEach(item, lista) {
                novas.push_back({RedeWatcher::Campo(item, "id"), RedeWatcher::Campo(item, "titulo"),
                                 RedeWatcher::Campo(item, "situacao"), RedeWatcher::Campo(item, "ha"),
                                 RedeWatcher::Campo(item, "ultima")});
            }
            cJSON_Delete(raiz);
            bool mudou = novas.size() != sessoes_.size();
            for (size_t k = 0; !mudou && k < novas.size(); k++) {
                mudou = novas[k].id != sessoes_[k].id || novas[k].situacao != sessoes_[k].situacao ||
                        novas[k].ha != sessoes_[k].ha;
            }
            if (mudou && tela_ == Tela::Lista) {
                sessoes_ = std::move(novas);
                int sel = std::max(0, std::min(c.painel.Selecionado() - 3, (int)sessoes_.size() - 1));
                MostrarLista(c, sel);
            }
        }
    }

    // O servidor manda a situação em português (é o valor comparado aqui e em IconeSituacao); a tela mostra no idioma do build
    static std::string RotuloSituacao(const std::string& s) {
        if (s == "Esperando você") return TR("Esperando você", "Waiting for you", "等你回复", "Esperándote");
        if (s == "Trabalhando") return TR("Trabalhando", "Working", "工作中", "Trabajando");
        if (s == "Subagentes") return TR("Subagentes", "Subagents", "子代理运行中", "Subagentes");
        if (s == "Concluída") return TR("Concluída", "Done", "已完成", "Terminada");
        if (s == "Parada") return TR("Parada", "Idle", "空闲", "Parada");
        return s;
    }

    static const char* IconeSituacao(const std::string& s) {
        if (s == "Esperando você") return MATERIAL_SYMBOLS_WARNING;
        if (s == "Trabalhando") return MATERIAL_SYMBOLS_PROGRESS_ACTIVITY;
        if (s == "Subagentes") return MATERIAL_SYMBOLS_PROGRESS_ACTIVITY;  // em execução: spinner
        if (s == "Concluída") return MATERIAL_SYMBOLS_CHECK_CIRCLE;
        return MATERIAL_SYMBOLS_SCHEDULE;
    }

private:
    enum class Tela { Carregando, Erro, Aviso, Lista, Detalhe, Pergunta, Resultado, NaoAchei };
    enum class Pedido { Nada, Lista, Sessao, Responder };
    struct Sessao {
        std::string id, titulo, situacao, ha, ultima;
    };
    struct Opcao {
        int n;
        std::string texto;
        bool marcada = false;
    };
    struct Pergunta {
        std::string texto;
        bool multipla = false;
        std::vector<Opcao> opcoes;
    };
    std::atomic<Tela> tela_{Tela::Carregando};
    std::atomic<Pedido> pedido_{Pedido::Nada};
    std::atomic<bool> refrescar_{false};
    std::vector<Sessao> sessoes_;
    int atual_ = 0;
    std::string ir_para_;
    bool sessoes_atualizar_ = false;
    std::string ir_titulo_;    // id da sessão a abrir depois de carregar a lista (vindo de um aviso)
    std::string mensagens_;  // últimas mensagens da sessão aberta
    Pergunta pergunta_;      // pergunta aberta na sessão (texto vazio = nenhuma)
    std::vector<int> escolhas_;

    static std::string Codificar(const std::string& texto) {
        static const char* hex = "0123456789ABCDEF";
        std::string saida;
        for (unsigned char ch : texto) {
            if (isalnum(ch) || ch == '-' || ch == '_' || ch == '.' || ch == '~') {
                saida += (char)ch;
            } else {
                saida += '%';
                saida += hex[ch >> 4];
                saida += hex[ch & 15];
            }
        }
        return saida;
    }

    void BuscarLista(ContextoApps& c) {
        std::string corpo;
        // Lista que veio com a última consulta de avisos (até 60 s): abre sem esperar a rede
        bool do_cache = !sessoes_atualizar_ && ContextoApps::Agora() - ContextoApps::sessoes_quando <= 60 &&
                        !ContextoApps::sessoes_json.empty();
        sessoes_atualizar_ = false;
        if (do_cache) {
            corpo = ContextoApps::sessoes_json;
            refrescar_ = true;  // mostra o cache já e busca a lista fresca por trás
        } else if (!RedeWatcher::Pedir("GET", "/watcher/sessoes", "", corpo)) {
            tela_ = Tela::Erro;
            c.painel.MostrarStatus("Claude Code", PainelWatcher::Status::Erro, TR("Não consegui falar com o Mac agora.", "Couldn't reach the Mac right now.", "现在无法连接 Mac。", "No he podido hablar con el Mac."),
                                   {TR("Voltar", "Back", "返回", "Volver")});
            return;
        }
        sessoes_.clear();
        cJSON* raiz = cJSON_Parse(corpo.c_str());
        cJSON* lista = raiz ? cJSON_GetObjectItem(raiz, "sessoes") : nullptr;
        cJSON* item = nullptr;
        cJSON_ArrayForEach(item, lista) {
            sessoes_.push_back({RedeWatcher::Campo(item, "id"), RedeWatcher::Campo(item, "titulo"),
                                RedeWatcher::Campo(item, "situacao"), RedeWatcher::Campo(item, "ha"),
                                RedeWatcher::Campo(item, "ultima")});
        }
        cJSON_Delete(raiz);
        if (!ir_para_.empty() || !ir_titulo_.empty()) {  // veio de um aviso: abre direto a sessão
            std::string alvo, titulo;
            alvo.swap(ir_para_);
            titulo.swap(ir_titulo_);
            for (int i = 0; i < (int)sessoes_.size(); i++) {
                if (!alvo.empty() && sessoes_[i].id == alvo) {
                    AbrirSessao(c, i);
                    return;
                }
            }
            // Sem id (avisos antigos) ou id mudou: procura pelo título (um pode estar cortado)
            for (int i = 0; i < (int)sessoes_.size() && !titulo.empty(); i++) {
                const auto& t = sessoes_[i].titulo;
                if (t.rfind(titulo.substr(0, t.size()), 0) == 0 || titulo.rfind(t, 0) == 0) {
                    AbrirSessao(c, i);
                    return;
                }
            }
            // A sessão do aviso não existe mais: diz isso em vez de largar a pessoa na lista sem explicação
            tela_ = Tela::NaoAchei;
            c.painel.MostrarStatus(titulo.empty() ? "Claude Code" : titulo, PainelWatcher::Status::Erro,
                                   TR("Essa sessão não está mais na lista. Ela pode ter sido encerrada.",
                                      "That session is no longer in the list. It may have been closed.",
                                      "该会话已不在列表中，可能已被关闭。",
                                      "Esa sesión ya no está en la lista. Puede haber sido cerrada."),
                                   {TR("Ver sessões", "See sessions", "查看会话", "Ver sesiones"),
                                    TR("Fechar", "Close", "关闭", "Cerrar")});
            return;
        }
        MostrarLista(c, 0);
    }

    void AbrirSessao(ContextoApps& c, int indice) {
        atual_ = indice;
        tela_ = Tela::Carregando;
        c.painel.MostrarStatus(sessoes_[atual_].titulo, PainelWatcher::Status::Carregando, TR("Lendo as mensagens…", "Reading messages…", "正在读取消息…", "Leyendo los mensajes…"), {}, "reading");
        pedido_ = Pedido::Sessao;
    }

    // Últimas mensagens (Você / Claude), da mais antiga para a mais recente, e a pergunta aberta, se houver
    void LerSessao(ContextoApps& c) {
        const auto& s = sessoes_[atual_];
        std::string base = "/watcher/sessoes/" + Codificar(s.id);
        mensagens_.clear();
        bool tem_pergunta = false;
        std::string corpo;
        if (RedeWatcher::Pedir("GET", base + "/mensagens", "", corpo)) {
            cJSON* raiz = cJSON_Parse(corpo.c_str());
            cJSON* lista = raiz ? cJSON_GetObjectItem(raiz, "mensagens") : nullptr;
            cJSON* m = nullptr;
            cJSON_ArrayForEach(m, lista) {
                std::string hora = RedeWatcher::Campo(m, "hora");
                mensagens_ += (mensagens_.empty() ? "" : "\n\n") + RedeWatcher::Campo(m, "quem") +
                              (hora.empty() ? "" : " · " + hora) + "\n" + RedeWatcher::Campo(m, "texto");
            }
            tem_pergunta = raiz && cJSON_GetObjectItem(raiz, "pergunta") != nullptr;
            if (tem_pergunta) {
                LerPerguntaDe(raiz);
            }
            cJSON_Delete(raiz);
        }
        if (mensagens_.empty()) {
            mensagens_ = s.ultima.empty() ? TR("Sem mensagem registrada.", "No messages recorded.", "没有记录的消息。", "Sin mensajes registrados.") : s.ultima;
        }
        if (!tem_pergunta) {
            LerPergunta(base);
        }
        MostrarDetalhe(c);
    }

    void LerPergunta(const std::string& base) {
        pergunta_ = Pergunta();
        std::string corpo;
        if (!RedeWatcher::Pedir("GET", base + "/pergunta", "", corpo)) {
            return;
        }
        cJSON* raiz = cJSON_Parse(corpo.c_str());
        LerPerguntaDe(raiz);
        cJSON_Delete(raiz);
    }

    void LerPerguntaDe(cJSON* raiz) {
        pergunta_ = Pergunta();
        cJSON* p = raiz ? cJSON_GetObjectItem(raiz, "pergunta") : nullptr;
        if (cJSON_IsObject(p)) {
            pergunta_.texto = RedeWatcher::Campo(p, "texto");
            pergunta_.multipla = cJSON_IsTrue(cJSON_GetObjectItem(p, "multipla"));
            cJSON* op = nullptr;
            cJSON_ArrayForEach(op, cJSON_GetObjectItem(p, "opcoes")) {
                if (cJSON_IsTrue(cJSON_GetObjectItem(op, "livre"))) {
                    continue;  // resposta livre: só por voz
                }
                pergunta_.opcoes.push_back({RedeWatcher::Numero(op, "n", 0), RedeWatcher::Campo(op, "texto")});
            }
            if (pergunta_.texto.empty()) {
                pergunta_.texto = TR("Pergunta da sessão", "Session question", "会话提问", "Pregunta de la sesión");
            }
            if (pergunta_.opcoes.empty()) {
                pergunta_ = Pergunta();
            }
        }
    }

    void MostrarLista(ContextoApps& c, int selecionar) {
        tela_ = Tela::Lista;
        std::vector<PainelWatcher::Item> itens = {{TR("Voltar", "Back", "返回", "Volver"), "", MATERIAL_SYMBOLS_ARROW_BACK},
                                                  {TR("Resumir todas", "Summarize all", "全部汇总", "Resumir todas"),
                                                   TR("O ", "", "", "El ") + AgenteWatcher::Nome() +
                                                       TR(" fala o estado de cada sessão", " reads each session's status",
                                                          " 播报每个会话的状态", " dice el estado de cada sesión"),
                                                   MATERIAL_SYMBOLS_HEADPHONES},
                                                  {TR("Nova sessão", "New session", "新会话", "Nueva sesión"),
                                                   TR("Diga o projeto e o que pedir", "Say the project and the request",
                                                      "说出项目和要做的事", "Di el proyecto y qué pedir"),
                                                   MATERIAL_SYMBOLS_MIC}};
        for (const auto& s : sessoes_) {
            itens.push_back({s.titulo, RotuloSituacao(s.situacao) + (s.ha.empty() ? std::string() : TR(" · há ", " · ", " · ", " · hace ") + s.ha + TR("", " ago", "前", "")), IconeSituacao(s.situacao)});
        }
        c.painel.MostrarLista("Claude Code", itens, selecionar + 3);
    }

    void MostrarDetalhe(ContextoApps& c) {
        tela_ = Tela::Detalhe;
        const auto& s = sessoes_[atual_];
        std::string texto = RotuloSituacao(s.situacao) + (s.ha.empty() ? std::string() : TR(" · há ", " · ", " · ", " · hace ") + s.ha + TR("", " ago", "前", "")) + "\n\n" + mensagens_;
        std::vector<std::string> botoes = {TR("Enviar pedido", "Send request", "发送请求", "Enviar petición"), TR("Voltar", "Back", "返回", "Volver")};
        if (!pergunta_.texto.empty()) {
            texto += TR("\n\nPergunta: ", "\n\nQuestion: ", "\n\n提问：", "\n\nPregunta: ") + pergunta_.texto;
            botoes.insert(botoes.begin(), TR("Responder", "Answer", "回答", "Responder"));
        }
        c.painel.MostrarTexto(s.titulo, texto, botoes);
        c.painel.RolarTextoParaFim();
    }

    // Lista: Voltar, "Ollie, leia as opções", opções (múltipla: marca/desmarca) e, na múltipla, Enviar
    void MostrarPergunta(ContextoApps& c, int selecionar) {
        tela_ = Tela::Pergunta;
        std::vector<PainelWatcher::Item> itens = {
            {TR("Voltar", "Back", "返回", "Volver"), "", MATERIAL_SYMBOLS_ARROW_BACK},
            {TR("Ouvir e responder", "Listen and answer", "收听并回答", "Escuchar y responder"),
             TR("O ", "", "", "El ") + AgenteWatcher::Nome() +
                 TR(" lê as opções", " reads the options", " 朗读选项", " lee las opciones"),
             MATERIAL_SYMBOLS_HEADPHONES}};
        for (const auto& op : pergunta_.opcoes) {
            const char* icone = !pergunta_.multipla ? MATERIAL_SYMBOLS_ARROW_FORWARD
                                                    : (op.marcada ? MATERIAL_SYMBOLS_CHECK_CIRCLE : MATERIAL_SYMBOLS_STOP);
            itens.push_back({op.texto, pergunta_.multipla ? (op.marcada ? TR("Marcada", "Selected", "已选", "Marcada")
                                                                   : TR("Clique para marcar", "Click to select", "点击选择", "Pulsa para marcar")) : "",
                             icone});
        }
        if (pergunta_.multipla) {
            int marcadas = 0;
            for (const auto& op : pergunta_.opcoes) {
                marcadas += op.marcada ? 1 : 0;
            }
            itens.push_back({TR("Enviar", "Send", "发送", "Enviar"),
                             marcadas ? std::to_string(marcadas) + TR(" marcada(s)", " selected", " 项已选", " marcada(s)")
                                      : TR("Marque ao menos uma", "Select at least one", "至少选择一项", "Marca al menos una"),
                             MATERIAL_SYMBOLS_CHECK});
        }
        c.painel.MostrarLista(pergunta_.texto, itens, selecionar);
    }

    void Responder(ContextoApps& c, const std::vector<int>& escolhas) {
        escolhas_ = escolhas;
        tela_ = Tela::Carregando;
        c.painel.MostrarStatus(TR("Respondendo", "Answering", "正在回答", "Respondiendo"), PainelWatcher::Status::Carregando,
                               TR("Enviando a resposta à sessão…", "Sending answer to session…", "正在向会话发送回答…", "Enviando la respuesta…"), {}, "teclando");
        pedido_ = Pedido::Responder;
    }

    void EnviarResposta(ContextoApps& c) {
        std::string json = "{\"escolhas\":[";
        for (size_t k = 0; k < escolhas_.size(); k++) {
            json += (k ? "," : "") + std::to_string(escolhas_[k]);
        }
        json += "]}";
        std::string corpo;
        bool ok = RedeWatcher::Pedir("POST", "/watcher/sessoes/" + Codificar(sessoes_[atual_].id) + "/responder", json, corpo);
        cJSON* raiz = ok ? cJSON_Parse(corpo.c_str()) : nullptr;
        ok = ok && cJSON_IsTrue(cJSON_GetObjectItem(raiz, "ok"));
        std::string mensagem = RedeWatcher::Campo(raiz, "mensagem");
        cJSON_Delete(raiz);
        if (ok) {
            pergunta_ = Pergunta();  // respondida
        }
        tela_ = Tela::Resultado;
        c.painel.MostrarStatus(TR("Resposta", "Answer", "回答", "Respuesta"), ok ? PainelWatcher::Status::Sucesso : PainelWatcher::Status::Erro,
                               mensagem.empty() ? (ok ? TR("Resposta enviada.", "Answer sent.", "回答已发送。", "Respuesta enviada.")
                                                     : TR("Não consegui responder agora.", "Couldn't answer right now.", "现在无法回答。", "No he podido responder ahora.")) : mensagem,
                               {TR("Voltar", "Back", "返回", "Volver")});
    }
};

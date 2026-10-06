// App "Multica": controla o Multica do Mac de longe. Mostra o daemon local (liga/desliga), as issues
// abertas (lê a issue com os últimos comentários, comenta por voz e muda a situação), os autopilots
// (dispara um agora) e cria uma issue nova por voz. Rede no Tique (fora da trava da gaveta).
#pragma once

#include <atomic>
#include <cctype>
#include <vector>

#include "../idioma_watcher.h"
#include "../nucleo_apps.h"

class AppMultica : public AppWatcher {
public:
    const char* Nome() const override { return "Multica"; }
    const char* Id() const override { return "multica"; }
    const char* Icone() const override { return MATERIAL_SYMBOLS_CHECK_CIRCLE; }
    std::string Detalhe() const override { return TR("Issues, agentes e autopilots", "Issues, agents, autopilots", "Issue、智能体与自动驾驶", "Issues, agentes y autopilots"); }

    void Abrir(ContextoApps& c) override {
        tela_ = Tela::Carregando;
        c.painel.MostrarStatus("Multica", PainelWatcher::Status::Carregando, TR("Consultando o Multica…", "Checking Multica…", "正在查询 Multica…", "Consultando Multica…"));
        pedido_ = Pedido::Resumo;
    }

    void Girar(ContextoApps& c, int passo) override {
        if (tela_ != Tela::Carregando) {
            c.painel.Mover(passo);
        }
    }

    bool Clicar(ContextoApps& c) override {
        int i = c.painel.Selecionado();
        switch (tela_) {
            case Tela::Carregando:
                return true;
            case Tela::Erro:
                return false;
            case Tela::Lista: {
                int ni = (int)issues_.size();
                if (i == 0) {
                    return false;  // Voltar
                }
                if (i == 1) {  // issue nova por voz
                    c.Perguntar(TR("Quero criar uma issue no Multica. Pergunte o que precisa ser feito e para qual agente "
                                   "(veja os nomes com multica_agentes); depois confirme e use multica_criar_issue.",
                                   "I want to create an issue in Multica. Ask me what needs to be done and for which agent "
                                   "(check the names with multica_agentes); then confirm and use multica_criar_issue.",
                                   "我想在 Multica 里创建一个 issue。问我要做什么、交给哪个智能体"
                                   "（用 multica_agentes 查看名字）；然后确认并使用 multica_criar_issue。",
                                   "Quiero crear una issue en Multica. Pregúntame qué hay que hacer y para qué agente "
                                   "(mira los nombres con multica_agentes); luego confirma y usa multica_criar_issue."));
                    return true;
                }
                if (i == 2) {  // daemon
                    tela_ = Tela::Daemon;
                    c.painel.MostrarTexto(TR("Daemon local", "Local daemon", "本地守护进程", "Daemon local"),
                                          daemon_detalhe_ +
                                              (daemon_ ? TR("\n\nDesligar o daemon? Os agentes deste Mac param de pegar tarefas.",
                                                            "\n\nTurn off the daemon? This Mac's agents stop picking up tasks.",
                                                            "\n\n关闭守护进程？这台 Mac 上的智能体将不再领取任务。",
                                                            "\n\n¿Desactivar el daemon? Los agentes de este Mac dejan de tomar tareas.")
                                                       : TR("\n\nLigar o daemon? Os agentes deste Mac passam a pegar tarefas.",
                                                            "\n\nTurn on the daemon? This Mac's agents start picking up tasks.",
                                                            "\n\n开启守护进程？这台 Mac 上的智能体将开始领取任务。",
                                                            "\n\n¿Activar el daemon? Los agentes de este Mac empiezan a tomar tareas.")),
                                          {daemon_ ? TR("Desligar", "Turn off", "关闭", "Desactivar") : TR("Ligar", "Turn on", "开启", "Activar"),
                                           TR("Voltar", "Back", "返回", "Volver")});
                    return true;
                }
                if (i >= 3 && i < 3 + ni) {
                    atual_ = i - 3;
                    tela_ = Tela::Carregando;
                    c.painel.MostrarStatus(issues_[atual_].id, PainelWatcher::Status::Carregando,
                                           TR("Lendo a issue…", "Reading the issue…", "正在读取 issue…", "Leyendo la issue…"));
                    pedido_ = Pedido::Issue;
                    return true;
                }
                if (i >= 3 + ni && i < 3 + ni + (int)autopilots_.size()) {
                    atual_ = i - 3 - ni;
                    tela_ = Tela::Autopilot;
                    c.painel.MostrarTexto("Autopilot", autopilots_[atual_].titulo +
                                                           (autopilots_[atual_].detalhe.empty() ? "" : "\n" + autopilots_[atual_].detalhe) +
                                                           TR("\n\nDisparar agora?", "\n\nRun it now?", "\n\n现在运行？", "\n\n¿Lanzarlo ahora?"),
                                          {TR("Disparar", "Run", "运行", "Lanzar"), TR("Voltar", "Back", "返回", "Volver")});
                    return true;
                }
                return true;
            }
            case Tela::Issue:
                if (i == 0) {
                    MostrarAcoes(c);
                } else {
                    MostrarLista(c);
                }
                return true;
            case Tela::Acoes: {
                if (i == 0) {  // Voltar para a issue
                    tela_ = Tela::Issue;
                    MostrarIssue(c);
                    return true;
                }
                const auto& s = issues_[atual_];
                if (i == 1) {
                    c.Perguntar(TR("Quero comentar na issue ", "I want to comment on the Multica issue ", "我想在 Multica 的 issue ",
                                   "Quiero comentar en la issue ") +
                                s.id +
                                TR(" do Multica. Pergunte o que devo escrever e, depois que eu responder, confirme e use multica_comentar.",
                                   ". Ask me what to write and, after I answer, confirm and use multica_comentar.",
                                   " 上评论。问我要写什么，我回答后先确认，再使用 multica_comentar。",
                                   " de Multica. Pregúntame qué escribir y, cuando responda, confirma y usa multica_comentar."));
                    return true;
                }
                int k = i - 2;
                if (k >= 0 && k < (int)situacoes_.size()) {
                    nova_situacao_ = situacoes_[k];
                    tela_ = Tela::Carregando;
                    c.painel.MostrarStatus(s.id, PainelWatcher::Status::Carregando, TR("Mudando a situação…", "Changing the status…", "正在更改状态…", "Cambiando el estado…"));
                    pedido_ = Pedido::Situacao;
                }
                return true;
            }
            case Tela::Daemon:
            case Tela::Autopilot:
                if (i == 0) {
                    bool daemon = tela_ == Tela::Daemon;
                    tela_ = Tela::Carregando;
                    c.painel.MostrarStatus(daemon ? TR("Daemon local", "Local daemon", "本地守护进程", "Daemon local") : "Autopilot",
                                           PainelWatcher::Status::Carregando,
                                           daemon ? (daemon_ ? TR("Desligando…", "Turning off…", "正在关闭…", "Desactivando…")
                                                             : TR("Ligando…", "Turning on…", "正在开启…", "Activando…"))
                                                  : TR("Disparando…", "Starting…", "正在运行…", "Lanzando…"));
                    pedido_ = daemon ? Pedido::Daemon : Pedido::Autopilot;
                } else {
                    MostrarLista(c);
                }
                return true;
            case Tela::Resultado:
                Atualizar(c);  // volta à lista atualizada
                return true;
        }
        return false;
    }

    bool Voltar(ContextoApps& c) override {
        if (tela_ == Tela::Acoes) {
            tela_ = Tela::Issue;
            MostrarIssue(c);
            return true;
        }
        if (tela_ == Tela::Issue || tela_ == Tela::Daemon || tela_ == Tela::Autopilot) {
            MostrarLista(c);
            return true;
        }
        if (tela_ == Tela::Resultado) {
            Atualizar(c);
            return true;
        }
        return false;
    }

    void Tique(ContextoApps& c) override {
        Pedido p = pedido_.exchange(Pedido::Nada);
        if (p == Pedido::Resumo) {
            BuscarResumo(c);
        } else if (p == Pedido::Issue) {
            LerIssue(c);
        } else if (p == Pedido::Situacao) {
            Executar(c, issues_[atual_].id, "/watcher/multica/issues/" + Codificar(issues_[atual_].id) + "/status",
                     "{\"status\":\"" + nova_situacao_ + "\"}");
        } else if (p == Pedido::Daemon) {
            Executar(c, TR("Daemon local", "Local daemon", "本地守护进程", "Daemon local"), "/watcher/multica/daemon",
                     daemon_ ? "{\"ligar\":false}" : "{\"ligar\":true}");
        } else if (p == Pedido::Autopilot) {
            Executar(c, "Autopilot", "/watcher/multica/autopilots/" + Codificar(autopilots_[atual_].id) + "/disparar", "{}");
        }
    }

private:
    enum class Tela { Carregando, Erro, Lista, Issue, Acoes, Daemon, Autopilot, Resultado };
    enum class Pedido { Nada, Resumo, Issue, Situacao, Daemon, Autopilot };
    struct Item {
        std::string id, titulo, detalhe, status;
    };
    std::atomic<Tela> tela_{Tela::Carregando};
    std::atomic<Pedido> pedido_{Pedido::Nada};
    std::vector<Item> issues_, autopilots_;
    std::vector<std::string> situacoes_;  // opções da tela de ações, na ordem mostrada
    std::string erro_, daemon_detalhe_, issue_texto_, nova_situacao_;
    bool daemon_ = false;
    int atual_ = 0;
    int cache_s_ = 30;  // depois de uma ação, a lista é pedida de novo sem cache

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

    // Chave da situação (comparada com o servidor) -> rótulo e ícone na tela
    static const char* Rotulo(const std::string& st) {
        if (st == "in_progress") return TR("Em andamento", "In progress", "进行中", "En curso");
        if (st == "in_review") return TR("Em revisão", "In review", "审核中", "En revisión");
        if (st == "blocked") return TR("Bloqueada", "Blocked", "已阻塞", "Bloqueada");
        if (st == "todo") return TR("A fazer", "To do", "待办", "Por hacer");
        if (st == "backlog") return "Backlog";
        if (st == "done") return TR("Concluída", "Done", "已完成", "Completada");
        return TR("Cancelada", "Cancelled", "已取消", "Cancelada");
    }

    static const char* IconeSituacao(const std::string& st) {
        if (st == "in_progress") return MATERIAL_SYMBOLS_PROGRESS_ACTIVITY;
        if (st == "in_review") return MATERIAL_SYMBOLS_EYEGLASSES;
        if (st == "blocked") return MATERIAL_SYMBOLS_WARNING;
        if (st == "done") return MATERIAL_SYMBOLS_CHECK_CIRCLE;
        if (st == "cancelled") return MATERIAL_SYMBOLS_CANCEL;
        return MATERIAL_SYMBOLS_SCHEDULE;
    }

    void Atualizar(ContextoApps& c) {
        tela_ = Tela::Carregando;
        c.painel.MostrarStatus("Multica", PainelWatcher::Status::Carregando, TR("Atualizando…", "Updating…", "正在更新…", "Actualizando…"));
        pedido_ = Pedido::Resumo;
    }

    static void LerItens(cJSON* lista, std::vector<Item>& destino) {
        destino.clear();
        cJSON* item = nullptr;
        cJSON_ArrayForEach(item, lista) {
            destino.push_back({RedeWatcher::Campo(item, "id"), RedeWatcher::Campo(item, "titulo"),
                               RedeWatcher::Campo(item, "detalhe"), RedeWatcher::Campo(item, "status")});
        }
    }

    void BuscarResumo(ContextoApps& c) {
        std::string corpo;
        bool ok = RedeWatcher::PedirCache("/watcher/multica", cache_s_, corpo);
        cache_s_ = 30;
        if (!ok) {
            tela_ = Tela::Erro;
            c.painel.MostrarStatus("Multica", PainelWatcher::Status::Erro, TR("Não consegui falar com o Mac agora.", "Couldn't reach the Mac right now.", "现在无法连接 Mac。", "No he podido hablar con el Mac."),
                                   {TR("Voltar", "Back", "返回", "Volver")});
            return;
        }
        cJSON* raiz = cJSON_Parse(corpo.c_str());
        cJSON* daemon = raiz ? cJSON_GetObjectItem(raiz, "daemon") : nullptr;
        daemon_ = cJSON_IsTrue(cJSON_GetObjectItem(daemon, "ligado"));
        daemon_detalhe_ = RedeWatcher::Campo(daemon, "detalhe");
        LerItens(raiz ? cJSON_GetObjectItem(raiz, "issues") : nullptr, issues_);
        LerItens(raiz ? cJSON_GetObjectItem(raiz, "autopilots") : nullptr, autopilots_);
        erro_ = RedeWatcher::Campo(raiz, "erro");
        cJSON_Delete(raiz);
        MostrarLista(c);
    }

    void MostrarLista(ContextoApps& c) {
        tela_ = Tela::Lista;
        std::vector<PainelWatcher::Item> itens = {
            {TR("Voltar", "Back", "返回", "Volver"), "", MATERIAL_SYMBOLS_ARROW_BACK},
            {TR("Nova issue", "New issue", "新 issue", "Nueva issue"),
             TR("Diga o que o agente deve fazer", "Say what the agent should do", "说出智能体要做什么", "Di qué debe hacer el agente"),
             MATERIAL_SYMBOLS_MIC},
            {TR("Daemon local", "Local daemon", "本地守护进程", "Daemon local"), daemon_detalhe_, MATERIAL_SYMBOLS_POWER_SETTINGS_NEW}};
        for (const auto& s : issues_) {
            itens.push_back({s.titulo, s.detalhe, IconeSituacao(s.status)});
        }
        for (const auto& a : autopilots_) {
            itens.push_back({a.titulo, a.detalhe.empty() ? "Autopilot" : a.detalhe, MATERIAL_SYMBOLS_REPEAT});
        }
        if (issues_.empty()) {
            itens.push_back({TR("Nenhuma issue aberta", "No open issues", "没有未完成的 issue", "Sin issues abiertas"), erro_, MATERIAL_SYMBOLS_INFO});
        }
        c.painel.MostrarLista("Multica", itens, 1);
    }

    void LerIssue(ContextoApps& c) {
        const auto& s = issues_[atual_];
        std::string corpo;
        issue_texto_.clear();
        if (RedeWatcher::Pedir("GET", "/watcher/multica/issues/" + Codificar(s.id), "", corpo)) {
            cJSON* raiz = cJSON_Parse(corpo.c_str());
            issue_texto_ = RedeWatcher::Campo(raiz, "texto");
            std::string st = RedeWatcher::Campo(raiz, "status");
            if (!st.empty()) {
                issues_[atual_].status = st;
            }
            cJSON_Delete(raiz);
        }
        tela_ = Tela::Issue;
        MostrarIssue(c);
    }

    void MostrarIssue(ContextoApps& c) {
        const auto& s = issues_[atual_];
        c.painel.MostrarTexto(s.id, issue_texto_.empty() ? TR("Não consegui ler a issue agora.", "Couldn't read the issue right now.", "现在无法读取 issue。", "No he podido leer la issue.") : issue_texto_,
                              {TR("Ações", "Actions", "操作", "Acciones"), TR("Voltar", "Back", "返回", "Volver")});
    }

    // Comentar por voz e mover para outra situação (a atual fica de fora)
    void MostrarAcoes(ContextoApps& c) {
        tela_ = Tela::Acoes;
        const auto& s = issues_[atual_];
        std::vector<PainelWatcher::Item> itens = {
            {TR("Voltar", "Back", "返回", "Volver"), "", MATERIAL_SYMBOLS_ARROW_BACK},
            {TR("Comentar", "Comment", "评论", "Comentar"), TR("O agente lê e reage", "The agent reads it and reacts", "智能体会阅读并回应", "El agente lo lee y reacciona"),
             MATERIAL_SYMBOLS_MIC}};
        situacoes_.clear();
        for (const char* st : {"done", "in_review", "in_progress", "todo", "blocked", "backlog", "cancelled"}) {
            if (s.status != st) {
                situacoes_.push_back(st);
                itens.push_back({std::string(TR("Mover para: ", "Move to: ", "移至：", "Mover a: ")) + Rotulo(st), "", IconeSituacao(st)});
            }
        }
        c.painel.MostrarLista(s.id, itens, 1);
    }

    void Executar(ContextoApps& c, const std::string& titulo, const std::string& rota, const std::string& conteudo) {
        std::string corpo;
        bool ok = RedeWatcher::Pedir("POST", rota, conteudo, corpo);
        cJSON* raiz = ok ? cJSON_Parse(corpo.c_str()) : nullptr;
        ok = ok && cJSON_IsTrue(cJSON_GetObjectItem(raiz, "ok"));
        std::string mensagem = RedeWatcher::Campo(raiz, "mensagem");
        cJSON_Delete(raiz);
        cache_s_ = 0;
        tela_ = Tela::Resultado;
        c.painel.MostrarStatus(titulo, ok ? PainelWatcher::Status::Sucesso : PainelWatcher::Status::Erro,
                               mensagem.empty() ? (ok ? TR("Pronto.", "Done.", "完成。", "Listo.")
                                                      : TR("Não consegui fazer isso agora.", "Couldn't do that right now.", "现在无法完成。", "No he podido hacerlo."))
                                                : mensagem,
                               {TR("Voltar", "Back", "返回", "Volver")});
    }
};

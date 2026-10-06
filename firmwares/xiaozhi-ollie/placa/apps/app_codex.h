// App "Codex": controla o Codex do Mac de longe. Mostra o controle remoto (ligado/desligado),
// as sessões do Codex (últimas mensagens e enviar mensagem por voz), as tarefas do Codex Cloud
// e cria uma tarefa nova por voz. Rede no Tique (fora da trava da gaveta).
#pragma once

#include <atomic>
#include <cctype>
#include <vector>

#include "../idioma_watcher.h"
#include "../nucleo_apps.h"

class AppCodex : public AppWatcher {
public:
    const char* Nome() const override { return "Codex"; }
    const char* Id() const override { return "codex"; }
    const char* Icone() const override { return PainelWatcher::kIconeCodex; }  // logo desenhado
    std::string Detalhe() const override { return TR("Sessões, nuvem e controle remoto", "Sessions, cloud, remote control", "会话、云端与远程控制", "Sesiones, nube y control remoto"); }

    void Abrir(ContextoApps& c) override {
        tela_ = Tela::Carregando;
        c.painel.MostrarStatus("Codex", PainelWatcher::Status::Carregando, TR("Consultando o Codex…", "Checking Codex…", "正在查询 Codex…", "Consultando Codex…"));
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
                int ns = (int)sessoes_.size();
                if (i == 0) {
                    return false;  // Voltar
                }
                if (i == 1) {  // tarefa nova por voz
                    c.Perguntar(TR("Quero começar uma tarefa nova no Codex do Mac. Pergunte em qual projeto e o que devo pedir; "
                                   "depois confirme e use codex_nova_sessao.",
                                   "I want to start a new task in Codex on the Mac. Ask me which project and what to request; "
                                   "then confirm and use codex_nova_sessao.",
                                   "我想在 Mac 上的 Codex 里开始一个新任务。问我是哪个项目、要提什么需求；"
                                   "然后确认并使用 codex_nova_sessao。",
                                   "Quiero empezar una tarea nueva en Codex del Mac. Pregúntame en qué proyecto y qué debo pedir; "
                                   "luego confirma y usa codex_nova_sessao."));
                    return true;
                }
                if (i == 2) {  // controle remoto
                    tela_ = Tela::Remoto;
                    c.painel.MostrarTexto(TR("Controle remoto", "Remote control", "远程控制", "Control remoto"),
                                          std::string(remoto_ ? TR("Está ligado. ", "It's on. ", "已开启。", "Está activado. ")
                                                              : TR("Está desligado. ", "It's off. ", "已关闭。", "Está desactivado. ")) +
                                              remoto_detalhe_ +
                                              (remoto_ ? TR("\n\nDesligar o controle remoto do Codex?", "\n\nTurn off Codex remote control?",
                                                            "\n\n关闭 Codex 远程控制？", "\n\n¿Desactivar el control remoto de Codex?")
                                                       : TR("\n\nLigar o controle remoto do Codex?", "\n\nTurn on Codex remote control?",
                                                            "\n\n开启 Codex 远程控制？", "\n\n¿Activar el control remoto de Codex?")),
                                          {remoto_ ? TR("Desligar", "Turn off", "关闭", "Desactivar") : TR("Ligar", "Turn on", "开启", "Activar"),
                                           TR("Voltar", "Back", "返回", "Volver")});
                    return true;
                }
                if (i >= 3 && i < 3 + ns) {
                    atual_ = i - 3;
                    tela_ = Tela::Carregando;
                    c.painel.MostrarStatus(sessoes_[atual_].titulo, PainelWatcher::Status::Carregando,
                                           TR("Lendo as mensagens…", "Reading messages…", "正在读取消息…", "Leyendo los mensajes…"));
                    pedido_ = Pedido::Sessao;
                    return true;
                }
                if (i >= 3 + ns && i < 3 + ns + (int)nuvem_.size()) {
                    atual_ = i - 3 - ns;
                    tela_ = Tela::Carregando;
                    c.painel.MostrarStatus(nuvem_[atual_].titulo, PainelWatcher::Status::Carregando,
                                           TR("Consultando a tarefa…", "Checking task…", "正在查询任务…", "Consultando la tarea…"));
                    pedido_ = Pedido::Nuvem;
                    return true;
                }
                return true;
            }
            case Tela::Sessao:
                if (i == 0) {
                    const auto& s = sessoes_[atual_];
                    c.Perguntar(TR("Quero mandar uma mensagem para a sessão do Codex \"", "I want to send a message to the Codex session \"",
                                   "我想给 Codex 会话\"", "Quiero enviar un mensaje a la sesión de Codex \"") +
                                s.titulo + "\" (id " + s.id +
                                TR("). Pergunte o que devo enviar e, depois que eu responder, confirme e use codex_enviar nessa sessão.",
                                   "). Ask me what to send and, after I answer, confirm and use codex_enviar on that session.",
                                   "）发送一条消息。问我要发送什么，我回答后先确认，再对该会话使用 codex_enviar。",
                                   "). Pregúntame qué debo enviar y, cuando responda, confirma y usa codex_enviar en esa sesión."));
                } else {
                    MostrarLista(c);
                }
                return true;
            case Tela::Remoto:
                if (i == 0) {
                    tela_ = Tela::Carregando;
                    c.painel.MostrarStatus(TR("Controle remoto", "Remote control", "远程控制", "Control remoto"), PainelWatcher::Status::Carregando,
                                           remoto_ ? TR("Desligando…", "Turning off…", "正在关闭…", "Desactivando…")
                                                   : TR("Ligando…", "Turning on…", "正在开启…", "Activando…"));
                    pedido_ = Pedido::Remoto;
                } else {
                    MostrarLista(c);
                }
                return true;
            case Tela::Texto:
            case Tela::Resultado:
                pedido_ = Pedido::Resumo;  // volta à lista atualizada
                tela_ = Tela::Carregando;
                c.painel.MostrarStatus("Codex", PainelWatcher::Status::Carregando, TR("Atualizando…", "Updating…", "正在更新…", "Actualizando…"));
                return true;
        }
        return false;
    }

    bool Voltar(ContextoApps& c) override {
        if (tela_ == Tela::Sessao || tela_ == Tela::Remoto || tela_ == Tela::Texto || tela_ == Tela::Resultado) {
            MostrarLista(c);
            return true;
        }
        return false;
    }

    void Tique(ContextoApps& c) override {
        Pedido p = pedido_.exchange(Pedido::Nada);
        if (p == Pedido::Resumo) {
            BuscarResumo(c);
        } else if (p == Pedido::Sessao) {
            LerSessao(c);
        } else if (p == Pedido::Nuvem) {
            LerNuvem(c);
        } else if (p == Pedido::Remoto) {
            AlternarRemoto(c);
        }
    }

private:
    enum class Tela { Carregando, Erro, Lista, Sessao, Remoto, Texto, Resultado };
    enum class Pedido { Nada, Resumo, Sessao, Nuvem, Remoto };
    struct Item {
        std::string id, titulo, detalhe;
    };
    std::atomic<Tela> tela_{Tela::Carregando};
    std::atomic<Pedido> pedido_{Pedido::Nada};
    std::vector<Item> sessoes_, nuvem_;
    std::string erro_nuvem_;
    bool remoto_ = false;
    std::string remoto_detalhe_;
    int atual_ = 0;

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

    static void LerItens(cJSON* lista, std::vector<Item>& destino) {
        destino.clear();
        cJSON* item = nullptr;
        cJSON_ArrayForEach(item, lista) {
            destino.push_back({RedeWatcher::Campo(item, "id"), RedeWatcher::Campo(item, "titulo"),
                               RedeWatcher::Campo(item, "detalhe")});
        }
    }

    void BuscarResumo(ContextoApps& c) {
        std::string corpo;
        if (!RedeWatcher::PedirCache("/watcher/codex", 30, corpo)) {
            tela_ = Tela::Erro;
            c.painel.MostrarStatus("Codex", PainelWatcher::Status::Erro, TR("Não consegui falar com o Mac agora.", "Couldn't reach the Mac right now.", "现在无法连接 Mac。", "No he podido hablar con el Mac."),
                                   {TR("Voltar", "Back", "返回", "Volver")});
            return;
        }
        cJSON* raiz = cJSON_Parse(corpo.c_str());
        cJSON* remoto = raiz ? cJSON_GetObjectItem(raiz, "remoto") : nullptr;
        remoto_ = cJSON_IsTrue(cJSON_GetObjectItem(remoto, "ligado"));
        remoto_detalhe_ = RedeWatcher::Campo(remoto, "detalhe");
        LerItens(raiz ? cJSON_GetObjectItem(raiz, "sessoes") : nullptr, sessoes_);
        LerItens(raiz ? cJSON_GetObjectItem(raiz, "nuvem") : nullptr, nuvem_);
        erro_nuvem_ = RedeWatcher::Campo(raiz, "erro_nuvem");
        cJSON_Delete(raiz);
        MostrarLista(c);
    }

    void MostrarLista(ContextoApps& c) {
        tela_ = Tela::Lista;
        std::vector<PainelWatcher::Item> itens = {
            {TR("Voltar", "Back", "返回", "Volver"), "", MATERIAL_SYMBOLS_ARROW_BACK},
            {TR("Nova tarefa", "New task", "新任务", "Nueva tarea"),
             TR("Diga o que o Codex deve fazer", "Say what Codex should do", "说出 Codex 要做什么", "Di qué debe hacer Codex"),
             MATERIAL_SYMBOLS_MIC},
            {TR("Controle remoto", "Remote control", "远程控制", "Control remoto"), remoto_ ? TR("Ligado", "On", "已开启", "Activado") : TR("Desligado", "Off", "已关闭", "Desactivado"),
             remoto_ ? MATERIAL_SYMBOLS_LOCK_OPEN : MATERIAL_SYMBOLS_LOCK}};
        for (const auto& s : sessoes_) {
            itens.push_back({s.titulo, s.detalhe, MATERIAL_SYMBOLS_ROBOT_2});
        }
        for (const auto& t : nuvem_) {
            itens.push_back({t.titulo, t.detalhe, MATERIAL_SYMBOLS_CLOUD_UPLOAD});
        }
        if (sessoes_.empty() && nuvem_.empty()) {
            itens.push_back({TR("Nenhuma sessão do Codex", "No Codex sessions", "没有 Codex 会话", "Sin sesiones de Codex"), erro_nuvem_, MATERIAL_SYMBOLS_INFO});
        }
        c.painel.MostrarLista("Codex", itens, 1);
    }

    void LerSessao(ContextoApps& c) {
        const auto& s = sessoes_[atual_];
        std::string corpo, texto;
        if (RedeWatcher::Pedir("GET", "/watcher/codex/sessoes/" + Codificar(s.id), "", corpo)) {
            cJSON* raiz = cJSON_Parse(corpo.c_str());
            cJSON* lista = raiz ? cJSON_GetObjectItem(raiz, "mensagens") : nullptr;  // fora do macro (sem ?: dentro)
            cJSON* m = nullptr;
            cJSON_ArrayForEach(m, lista) {
                std::string hora = RedeWatcher::Campo(m, "hora");
                texto += (texto.empty() ? "" : "\n\n") + RedeWatcher::Campo(m, "quem") + (hora.empty() ? "" : " · " + hora) +
                         "\n" + RedeWatcher::Campo(m, "texto");
            }
            cJSON_Delete(raiz);
        }
        tela_ = Tela::Sessao;
        c.painel.MostrarTexto(s.titulo, (s.detalhe.empty() ? "" : s.detalhe + "\n\n") +
                                            (texto.empty() ? TR("Sem mensagens registradas.", "No messages recorded.", "没有记录的消息。", "Sin mensajes registrados.") : texto),
                              {TR("Enviar mensagem", "Send message", "发送消息", "Enviar mensaje"), TR("Voltar", "Back", "返回", "Volver")});
        c.painel.RolarTextoParaFim();
    }

    void LerNuvem(ContextoApps& c) {
        const auto& t = nuvem_[atual_];
        std::string corpo, texto;
        if (RedeWatcher::Pedir("GET", "/watcher/codex/nuvem/" + Codificar(t.id), "", corpo)) {
            cJSON* raiz = cJSON_Parse(corpo.c_str());
            texto = RedeWatcher::Campo(raiz, "texto");
            cJSON_Delete(raiz);
        }
        tela_ = Tela::Texto;
        c.painel.MostrarTexto(t.titulo, texto.empty() ? TR("Não consegui ler a tarefa agora.", "Couldn't read the task right now.", "现在无法读取任务。", "No he podido leer la tarea.") : texto,
                              {TR("Voltar", "Back", "返回", "Volver")});
    }

    void AlternarRemoto(ContextoApps& c) {
        std::string corpo;
        bool ok = RedeWatcher::Pedir("POST", "/watcher/codex/remoto", remoto_ ? "{\"ligar\":false}" : "{\"ligar\":true,\"confirmado\":true}", corpo);
        cJSON* raiz = ok ? cJSON_Parse(corpo.c_str()) : nullptr;
        ok = ok && cJSON_IsTrue(cJSON_GetObjectItem(raiz, "ok"));
        std::string mensagem = RedeWatcher::Campo(raiz, "mensagem");
        cJSON_Delete(raiz);
        if (ok) {
            remoto_ = !remoto_;
        }
        tela_ = Tela::Resultado;
        c.painel.MostrarStatus(TR("Controle remoto", "Remote control", "远程控制", "Control remoto"), ok ? PainelWatcher::Status::Sucesso : PainelWatcher::Status::Erro,
                               mensagem.empty() ? (ok ? TR("Pronto.", "Done.", "完成。", "Listo.")
                                                      : TR("Não consegui mudar agora.", "Couldn't change it now.", "现在无法更改。", "No he podido cambiarlo."))
                                                : mensagem,
                               {TR("Voltar", "Back", "返回", "Volver")});
    }
};

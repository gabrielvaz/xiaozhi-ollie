// App "Sessões": lista as sessões do Claude Code/Codex no herdr, mostra as últimas mensagens (roláveis),
// manda um pedido por voz e responde pela tela às perguntas da sessão (escolha única ou múltipla,
// e pedidos de permissão), ou pede ao agente para ler as opções e responder por voz.
// Aberto por um aviso de "sessão esperando você", mostra o aviso com o botão "Ir para a sessão".
#pragma once

#include <atomic>
#include <cctype>
#include <vector>

#include "../nucleo_apps.h"

class AppSessoes : public AppWatcher {
public:
    const char* Nome() const override { return "Sessões"; }
    const char* Id() const override { return "sessoes"; }
    const char* Icone() const override { return MATERIAL_SYMBOLS_ROBOT_2; }
    std::string Detalhe() const override { return "Ver, ler e mandar pedidos"; }

    void Abrir(ContextoApps& c) override {
        ir_para_.clear();
        tela_ = Tela::Carregando;
        c.painel.MostrarStatus("Sessões", PainelWatcher::Status::Carregando, "Carregando sessões…");
        pedido_ = Pedido::Lista;
    }

    // Argumento "aviso\n<id>\n<título>\n<texto>": tela do aviso com "Ir para a sessão"
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
        if (argumento.rfind("ir\n", 0) == 0) {  // "ir\n<id>": abre direto a sessão (ex.: do histórico de avisos)
            ir_para_ = argumento.substr(3);
            tela_ = Tela::Carregando;
            c.painel.MostrarStatus("Sessões", PainelWatcher::Status::Carregando, "Abrindo a sessão…");
            pedido_ = Pedido::Lista;
            return;
        }
        if (partes.size() < 3 || partes[0] != "aviso") {
            Abrir(c);
            return;
        }
        ir_para_ = partes[1];
        tela_ = Tela::Aviso;
        c.painel.MostrarTexto(partes[2], argumento.substr(ini), {"Ir para a sessão", "Fechar"});
    }

    void Girar(ContextoApps& c, int passo) override {
        if (tela_ != Tela::Carregando && tela_ != Tela::Erro) {
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
            case Tela::Aviso:
                if (i == 0) {  // Ir para a sessão: carrega a lista e abre a sessão do aviso
                    tela_ = Tela::Carregando;
                    c.painel.MostrarStatus("Sessões", PainelWatcher::Status::Carregando, "Abrindo a sessão…");
                    pedido_ = Pedido::Lista;
                    return true;
                }
                c.fechar();
                return true;
            case Tela::Lista:
                if (i == 1) {  // resumo falado de todas as sessões ativas
                    c.Perguntar("Me dá uma atualização de todas as sessões ativas do Claude Code e do herdr: quantas estão "
                                "trabalhando, quais esperam aprovação, quais têm subagentes e o que cada uma concluiu ou está fazendo.");
                    return true;
                }
                if (i <= 0 || i > (int)sessoes_.size() + 1) {  // item 0 = Voltar
                    return false;
                }
                AbrirSessao(c, i - 2);
                return true;
            case Tela::Detalhe: {
                // Botões: [Responder,] Enviar pedido, Voltar
                int b = pergunta_.texto.empty() ? i + 1 : i;
                if (b == 0) {
                    MostrarPergunta(c, 2);
                } else if (b == 1) {
                    const auto& s = sessoes_[atual_];
                    c.Perguntar("Quero mandar um pedido para a sessão \"" + s.titulo + "\" (id " + s.id +
                                "). Pergunte o que devo enviar e, depois que eu responder, confirme e use sessao_instruir nessa sessão.");
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
                    c.Perguntar("A sessão \"" + s.titulo + "\" (id " + s.id + ") está me fazendo uma pergunta. Use "
                                "sessao_pergunta nessa sessão, leia a pergunta e as opções numeradas para mim, espere eu "
                                "responder, confirme a escolha e use sessao_escolher.");
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
        }
        return false;
    }

    bool Voltar(ContextoApps& c) override {
        if (tela_ == Tela::Pergunta || tela_ == Tela::Resultado) {
            MostrarDetalhe(c);
            return true;
        }
        if (tela_ == Tela::Detalhe) {
            MostrarLista(c, atual_);
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
        }
    }

    static const char* IconeSituacao(const std::string& s) {
        if (s == "Esperando você") return MATERIAL_SYMBOLS_WARNING;
        if (s == "Trabalhando") return MATERIAL_SYMBOLS_PROGRESS_ACTIVITY;
        if (s == "Subagentes") return MATERIAL_SYMBOLS_PROGRESS_ACTIVITY;  // em execução: spinner
        if (s == "Concluída") return MATERIAL_SYMBOLS_CHECK_CIRCLE;
        return MATERIAL_SYMBOLS_SCHEDULE;
    }

private:
    enum class Tela { Carregando, Erro, Aviso, Lista, Detalhe, Pergunta, Resultado };
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
    std::vector<Sessao> sessoes_;
    int atual_ = 0;
    std::string ir_para_;    // id da sessão a abrir depois de carregar a lista (vindo de um aviso)
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
        if (!RedeWatcher::Pedir("GET", "/watcher/sessoes", "", corpo)) {
            tela_ = Tela::Erro;
            c.painel.MostrarStatus("Sessões", PainelWatcher::Status::Erro, "Não consegui falar com o Mac agora.", {"Voltar"});
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
        if (!ir_para_.empty()) {  // veio de um aviso: abre direto a sessão
            std::string alvo;
            alvo.swap(ir_para_);
            for (int i = 0; i < (int)sessoes_.size(); i++) {
                if (sessoes_[i].id == alvo) {
                    AbrirSessao(c, i);
                    return;
                }
            }
        }
        MostrarLista(c, 0);
    }

    void AbrirSessao(ContextoApps& c, int indice) {
        atual_ = indice;
        tela_ = Tela::Carregando;
        c.painel.MostrarStatus(sessoes_[atual_].titulo, PainelWatcher::Status::Carregando, "Lendo as mensagens…");
        pedido_ = Pedido::Sessao;
    }

    // Últimas mensagens (Você / Claude), da mais antiga para a mais recente, e a pergunta aberta, se houver
    void LerSessao(ContextoApps& c) {
        const auto& s = sessoes_[atual_];
        std::string base = "/watcher/sessoes/" + Codificar(s.id);
        mensagens_.clear();
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
            cJSON_Delete(raiz);
        }
        if (mensagens_.empty()) {
            mensagens_ = s.ultima.empty() ? "Sem mensagem registrada." : s.ultima;
        }
        LerPergunta(base);
        MostrarDetalhe(c);
    }

    void LerPergunta(const std::string& base) {
        pergunta_ = Pergunta();
        std::string corpo;
        if (!RedeWatcher::Pedir("GET", base + "/pergunta", "", corpo)) {
            return;
        }
        cJSON* raiz = cJSON_Parse(corpo.c_str());
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
                pergunta_.texto = "Pergunta da sessão";
            }
            if (pergunta_.opcoes.empty()) {
                pergunta_ = Pergunta();
            }
        }
        cJSON_Delete(raiz);
    }

    void MostrarLista(ContextoApps& c, int selecionar) {
        tela_ = Tela::Lista;
        std::vector<PainelWatcher::Item> itens = {{"Voltar", "", MATERIAL_SYMBOLS_ARROW_BACK},
                                                  {"Resumir todas", "O Ollie fala o estado de cada sessão",
                                                   MATERIAL_SYMBOLS_HEADPHONES}};
        for (const auto& s : sessoes_) {
            itens.push_back({s.titulo, s.situacao + (s.ha.empty() ? "" : " · há " + s.ha), IconeSituacao(s.situacao)});
        }
        c.painel.MostrarLista("Sessões", itens, selecionar + 2);
    }

    void MostrarDetalhe(ContextoApps& c) {
        tela_ = Tela::Detalhe;
        const auto& s = sessoes_[atual_];
        std::string texto = s.situacao + (s.ha.empty() ? "" : " · há " + s.ha) + "\n\n" + mensagens_;
        std::vector<std::string> botoes = {"Enviar pedido", "Voltar"};
        if (!pergunta_.texto.empty()) {
            texto += "\n\nPergunta: " + pergunta_.texto;
            botoes.insert(botoes.begin(), "Responder");
        }
        c.painel.MostrarTexto(s.titulo, texto, botoes);
        c.painel.RolarTextoParaFim();
    }

    // Lista: Voltar, "Ollie, leia as opções", opções (múltipla: marca/desmarca) e, na múltipla, Enviar
    void MostrarPergunta(ContextoApps& c, int selecionar) {
        tela_ = Tela::Pergunta;
        std::vector<PainelWatcher::Item> itens = {
            {"Voltar", "", MATERIAL_SYMBOLS_ARROW_BACK},
            {"Ouvir e responder", "O Ollie lê as opções", MATERIAL_SYMBOLS_HEADPHONES}};
        for (const auto& op : pergunta_.opcoes) {
            const char* icone = !pergunta_.multipla ? MATERIAL_SYMBOLS_ARROW_FORWARD
                                                    : (op.marcada ? MATERIAL_SYMBOLS_CHECK_CIRCLE : MATERIAL_SYMBOLS_STOP);
            itens.push_back({op.texto, pergunta_.multipla ? (op.marcada ? "Marcada" : "Clique para marcar") : "",
                             icone});
        }
        if (pergunta_.multipla) {
            int marcadas = 0;
            for (const auto& op : pergunta_.opcoes) {
                marcadas += op.marcada ? 1 : 0;
            }
            itens.push_back({"Enviar", marcadas ? std::to_string(marcadas) + " marcada(s)" : "Marque ao menos uma",
                             MATERIAL_SYMBOLS_CHECK});
        }
        c.painel.MostrarLista(pergunta_.texto, itens, selecionar);
    }

    void Responder(ContextoApps& c, const std::vector<int>& escolhas) {
        escolhas_ = escolhas;
        tela_ = Tela::Carregando;
        c.painel.MostrarStatus("Respondendo", PainelWatcher::Status::Carregando, "Enviando a resposta à sessão…");
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
        c.painel.MostrarStatus("Resposta", ok ? PainelWatcher::Status::Sucesso : PainelWatcher::Status::Erro,
                               mensagem.empty() ? (ok ? "Resposta enviada." : "Não consegui responder agora.") : mensagem,
                               {"Voltar"});
    }
};

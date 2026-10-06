// Serviço (sem tela na gaveta): saudação com o nome do usuário na tela de espera.
// Aparece ao ligar (depois do logo) e ao voltar para a espera (sempre uma saudação do período: bom dia,
// boa tarde...) e troca a cada 3 min por uma frase aleatória (saudações e frases soltas, sem repetir a anterior). O Clawd troca de expressão/pose a cada 40–90 s,
// com poses que combinam com a hora do dia (sem Wi-Fi: "offline"; bateria no fim: "sad"). Também informa ao servidor o nome do agente.
#pragma once

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <string>
#include <vector>

#include <esp_random.h>

#include "../agente_watcher.h"
#include "../idioma_watcher.h"
#include "../nucleo_apps.h"

class ServicoSaudacao : public AppWatcher {
public:
    const char* Nome() const override { return TR("Saudação", "Greeting", "问候", "Saludo"); }
    bool Visivel() const override { return false; }
    void Abrir(ContextoApps& c) override {}
    bool Clicar(ContextoApps& c) override { return false; }

    void Fundo(ContextoApps& c) override {
        int agora = ContextoApps::Agora();
        SincronizarPerfil(agora);
        if (ContextoApps::App().GetDeviceState() != kDeviceStateIdle) {
            ocioso_desde_ = -1;
            return;
        }
        if (ocioso_desde_ < 0) {
            ocioso_desde_ = agora;
            proxima_ = agora + 2;  // logo depois de limpar a tela da conversa
            proxima_pose_ = agora + 20;
        }
        if (c.gaveta_aberta && c.gaveta_aberta()) {
            return;
        }
        // Não cobre um aviso que ainda está na tela: só os que chegaram depois de entrar na espera
        // (ao voltar para a espera a tela é limpa, então um aviso anterior já sumiu) e por até 10 min
        int aviso = ContextoApps::ultimo_aviso;
        if (aviso >= ocioso_desde_ && agora - aviso < 600) {
            proxima_ = std::max(proxima_, aviso + 600);
            return;
        }
        // Sessão do Claude Code rodando: Clawd trabalhando e uma frase de progresso no estilo do Claude Code
        if (ContextoApps::atividade_n > 0) {
            trabalhando_ = true;
            if (agora >= proxima_trabalho_) {
                proxima_trabalho_ = agora + 5;
                MostrarTrabalho(agora >= proxima_pose_);
                if (agora >= proxima_pose_) {
                    proxima_pose_ = agora + 20;
                }
            }
            return;
        }
        if (trabalhando_) {  // acabou: volta para a saudação na hora
            trabalhando_ = false;
            proxima_ = proxima_pose_ = agora;
        }
        if (agora >= proxima_pose_) {
            proxima_pose_ = agora + 40 + (int)(esp_random() % 51);  // 40 a 90 s
            std::string pose = Pose();
            ContextoApps::App().Schedule([pose]() { Board::GetInstance().GetDisplay()->SetEmotion(pose.c_str()); });
        }
        if (agora < proxima_) {
            return;
        }
        proxima_ = agora + kTrocaS;
        proxima_pose_ = agora + 40;  // a pose nova acompanha a frase nova
        std::string pose = Pose();
        std::string frase = Frase(saudou_desde_ != ocioso_desde_);  // entrou na espera: saudação
        saudou_desde_ = ocioso_desde_;
        ContextoApps::App().Schedule([pose]() { Board::GetInstance().GetDisplay()->SetEmotion(pose.c_str()); });
        ContextoApps::App().Schedule([frase]() {
            Board::GetInstance().GetDisplay()->SetChatMessage("saudacao", frase.c_str());
        });
    }

private:
    static constexpr int kTrocaS = 3 * 60;
    int ocioso_desde_ = -1;
    int saudou_desde_ = -2;  // ocioso_desde_ da última saudação (a 1ª frase de cada espera é saudação)
    int proxima_ = 0;
    int perfil_em_ = 0;
    bool perfil_ok_ = false;
    std::string ultima_frase_;
    int proxima_pose_ = 0;
    int proxima_trabalho_ = 0;
    bool trabalhando_ = false;
    int verbo_ = -1;

    // "Codando…" + a sessão que está rodando (e quantas mais); troca a pose de trabalho quando pedido
    void MostrarTrabalho(bool trocar_pose) {
        static const char* const kVerbos[] = {
            TR("Codando", "Coding", "编码中", "Programando"), TR("Pensando", "Thinking", "思考中", "Pensando"),
            TR("Ruminando", "Pondering", "琢磨中", "Rumiando"), TR("Refatorando", "Refactoring", "重构中", "Refactorizando"),
            TR("Depurando", "Debugging", "调试中", "Depurando"), TR("Compilando", "Compiling", "编译中", "Compilando"),
            TR("Arquitetando", "Architecting", "构思中", "Diseñando"), TR("Tecendo", "Weaving", "编织中", "Tejiendo"),
            TR("Cozinhando", "Cooking", "烹饪中", "Cocinando"), TR("Lapidando", "Polishing", "打磨中", "Puliendo"),
            TR("Orquestrando", "Orchestrating", "编排中", "Orquestando"), TR("Destrinchando", "Untangling", "梳理中", "Desenredando")};
        constexpr int n = sizeof(kVerbos) / sizeof(kVerbos[0]);
        constexpr int kCozinhando = 8;  // posição de "Cozinhando" em kVerbos
        verbo_ = (verbo_ + 1 + (int)(esp_random() % (n - 1))) % n;  // nunca repete o anterior
        int outras = ContextoApps::atividade_n - 1;
        // Nome da sessão numa linha (cabem 3 na tela de espera) e, embaixo, há quanto tempo ela está rodando
        std::string titulo = ContextoApps::atividade_titulo;
        if (titulo.size() > 18) {
            size_t corte = 17;
            while (corte > 0 && (static_cast<unsigned char>(titulo[corte]) & 0xC0) == 0x80) {
                corte--;  // não corta um caractere acentuado ao meio
            }
            titulo = titulo.substr(0, corte) + "…";
        }
        std::string frase = std::string("* ") + kVerbos[verbo_] + "…\n" + titulo +
                            (outras > 0 ? " +" + std::to_string(outras) : "");
        if (ContextoApps::atividade_inicio >= 0) {
            frase += "\n" + Rodando(ContextoApps::Agora() - ContextoApps::atividade_inicio);
        }
        ContextoApps::App().Schedule([frase]() {
            Board::GetInstance().GetDisplay()->SetChatMessage("saudacao", frase.c_str());
        });
        if (verbo_ == kCozinhando) {  // "Cozinhando…": o Clawd de chapéu de chef, mexendo a panela
            ContextoApps::App().Schedule([]() { Board::GetInstance().GetDisplay()->SetEmotion("cozinhando"); });
        } else if (trocar_pose) {
            static const char* const kPoses[] = {"codando", "teclando", "working", "searching", "reading", "bug", "rocket"};
            const char* pose = kPoses[esp_random() % (sizeof(kPoses) / sizeof(kPoses[0]))];
            ContextoApps::App().Schedule([pose]() { Board::GetInstance().GetDisplay()->SetEmotion(pose); });
        }
    }
    std::string ultima_pose_;

    // "rodando há 45 s", "rodando há 3 min 20 s", "rodando há 1 h 05 min"
    static std::string Rodando(int s) {
        s = std::max(0, s);
        std::string un_s = TR(" s", " s", " 秒", " s"), un_min = TR(" min", " min", " 分钟", " min"),
                    un_h = TR(" h", " h", " 小时", " h");
        std::string tempo;
        if (s < 60) {
            tempo = std::to_string(s) + un_s;
        } else if (s < 3600) {
            tempo = std::to_string(s / 60) + un_min + (s < 600 ? " " + std::to_string(s % 60) + un_s : "");
        } else {
            char mm[4];
            snprintf(mm, sizeof(mm), "%02d", s % 3600 / 60);
            tempo = std::to_string(s / 3600) + un_h + " " + mm + un_min;
        }
        return TR("rodando há ", "running for ", "已运行 ", "en marcha hace ") + tempo;
    }

    // Pose do Clawd para a espera: um conjunto comum mais poses do período, sem repetir a anterior
    std::string Pose() {
        if (!RedeWatcher::Online()) {
            return "offline";  // sem Wi-Fi: Clawd tentando encaixar o cabo
        }
        int nivel = 100;
        bool carregando = false, descarregando = false;
        if (Board::GetInstance().GetBatteryLevel(nivel, carregando, descarregando) && descarregando && nivel <= 10) {
            return "sad";  // bateria quase no fim
        }
        std::vector<const char*> poses = {"neutral", "happy", "winking", "cool", "relaxed", "waving",
                                          "confident", "thinking", "idea", "music", "nerd", "skate"};
        time_t t = time(nullptr);
        struct tm agora;
        localtime_r(&t, &agora);
        if (agora.tm_year + 1900 >= 2025) {
            int h = agora.tm_hour;
            if (h >= 5 && h < 12) {
                poses.insert(poses.end(), {"coffee", "coffee", "sunny", "running"});
            } else if (h >= 12 && h < 18) {
                poses.insert(poses.end(), {"reading", "working", "searching", "rocket", "skate"});
            } else if (h >= 18) {
                poses.insert(poses.end(), {"music", "relaxed", "party", "sleepy"});
            } else {
                poses.insert(poses.end(), {"sleepy", "sleepy", "relaxed", "coffee"});
            }
        }
        std::string escolhida = poses[esp_random() % poses.size()];
        if (escolhida == ultima_pose_) {
            escolhida = poses[(esp_random() + 1) % poses.size()];
        }
        ultima_pose_ = escolhida;
        return escolhida;
    }

    // Avisa o servidor do nome do agente e pega o nome do usuário (guardado para usar sem internet)
    void SincronizarPerfil(int agora) {
        if (perfil_ok_ || agora < perfil_em_ || !RedeWatcher::Online()) {
            return;
        }
        perfil_em_ = agora + 60;
        std::string corpo;
        if (!RedeWatcher::Pedir("GET", "/watcher/perfil?agente=" + AgenteWatcher::Nome(), "", corpo)) {
            return;
        }
        cJSON* raiz = cJSON_Parse(corpo.c_str());
        std::string usuario = RedeWatcher::Campo(raiz, "usuario");
        cJSON_Delete(raiz);
        if (!usuario.empty() && usuario != AgenteWatcher::Usuario()) {
            AgenteWatcher::DefinirUsuario(usuario);
            proxima_ = 0;  // já mostra a saudação com o nome
            saudou_desde_ = -2;
        }
        perfil_ok_ = true;
    }

    std::string Frase(bool saudacao) {
        std::string nome = AgenteWatcher::Usuario();
        // Frases do idioma do build: gerais e por período; o nome entra no lugar de "{}" sempre como vocativo
        // separado por vírgula (", " ou a chinesa "，"), para poder tirar quando não há nome
#if defined(CONFIG_LANGUAGE_EN_US)
        std::vector<std::string> frases = {
            "Hi, {}! How's it going?", "What are we doing today, {}?", "How can I help, {}?",
            "Hey, {}, how are you?", "Ready to get started, {}?", "I'm right here, {}. Just ask.",
        };
        std::vector<std::string> manha = {"Good morning, {}! Had your coffee?", "Morning, {}! Ready for the day?"};
        std::vector<std::string> tarde = {"Good afternoon, {}! How's your day?", "Good afternoon, {}. Need anything?"};
        std::vector<std::string> noite = {"Good evening, {}! Still going?", "Good evening, {}. How was your day?"};
        std::vector<std::string> madrugada = {"Hey, {}, late night grind?", "Still up, {}? I'm here."};
        std::vector<std::string> soltas = {
            "Did you know? The first computer bug was a real moth.", "Time for a sip of water?",
            "Stretch your back, {}. I'll keep watch.", "Small commits, happy merges.",
            "Your agents are in good hands.", "Ask me about your sessions anytime.",
            "Tip: three clicks on the wheel open the apps.", "Done is better than perfect.",
            "Look away from the screen for 20 seconds.", "One step at a time, {}.",
            "I'm keeping an eye on everything.", "Need a meeting summary? I can record it.",
        };
#elif defined(CONFIG_LANGUAGE_ZH_CN)
        std::vector<std::string> frases = {
            "你好，{}，最近好吗？", "{}，今天想做点什么？", "有什么可以帮你的，{}？",
            "嗨，{}！准备开工了吗？", "{}，需要我做点什么吗？", "{}，我在这儿，随时叫我。",
        };
        std::vector<std::string> manha = {"早上好，{}！喝咖啡了吗？", "早上好，{}！今天也加油！"};
        std::vector<std::string> tarde = {"下午好，{}！今天过得怎样？", "下午好，{}。需要帮忙吗？"};
        std::vector<std::string> noite = {"晚上好，{}！还在忙吗？", "晚上好，{}。今天过得如何？"};
        std::vector<std::string> madrugada = {"{}，还没睡呀？我在这儿。", "夜深了，{}，还在忙吗？"};
        std::vector<std::string> soltas = {
            "你知道吗？第一个计算机 bug 是一只真的飞蛾。", "要不要喝口水？",
            "{}，伸个懒腰吧，我帮你盯着。", "小步提交，合并更顺。",
            "你的助手们都很靠谱。", "随时问我会话的进展。",
            "小提示：滚轮连按三下打开应用。", "完成比完美更重要。",
            "让眼睛离开屏幕 20 秒吧。", "{}，一步一步来。",
            "我一直在帮你看着。", "要会议纪要吗？我可以录音。",
        };
#elif defined(CONFIG_LANGUAGE_ES_ES)
        std::vector<std::string> frases = {
            "Hola, {}, ¿qué tal?", "¿Qué hacemos hoy, {}?", "¿En qué te ayudo, {}?",
            "¡Ey, {}! ¿Cómo estás?", "¿Empezamos, {}?", "Aquí estoy, {}. Solo llámame.",
        };
        std::vector<std::string> manha = {"¡Buenos días, {}! ¿Ya has tomado café?", "¡Buenos días, {}! ¿Empezamos el día?"};
        std::vector<std::string> tarde = {"¡Buenas tardes, {}! ¿Qué tal el día?", "Buenas tardes, {}. ¿Necesitas algo?"};
        std::vector<std::string> noite = {"¡Buenas noches, {}! ¿Aún con energía?", "Buenas noches, {}. ¿Qué tal el día?"};
        std::vector<std::string> madrugada = {"Ey, {}, ¿madrugada productiva?", "¿Aún despierto, {}? Aquí estoy."};
        std::vector<std::string> soltas = {
            "¿Sabías? El primer bug fue una polilla de verdad.", "¿Un trago de agua?",
            "Estira la espalda, {}. Yo vigilo.", "Commits pequeños, merges felices.",
            "Tus agentes están en buenas manos.", "Pregúntame por tus sesiones cuando quieras.",
            "Truco: tres clics en la rueda abren las apps.", "Hecho es mejor que perfecto.",
            "Mira lejos de la pantalla 20 segundos.", "Paso a paso, {}.",
            "Lo estoy vigilando todo.", "¿Resumen de reunión? Puedo grabarla.",
        };
#else
        std::vector<std::string> frases = {
            "Olá, tudo bem, {}?", "O que vamos fazer hoje, {}?", "Como posso te ajudar, {}?",
            "Opaaa, {}, como você está?", "E aí, {}? Bora começar?", "Tô por aqui, {}. É só chamar.",
        };
        std::vector<std::string> manha = {"Bom dia, {}! Café já tomado?", "Bom dia, {}! Bora começar o dia?"};
        std::vector<std::string> tarde = {"Boa tarde, {}! Como vai o dia?", "Boa tarde, {}. Precisa de algo?"};
        std::vector<std::string> noite = {"Boa noite, {}! Ainda no gás?", "Boa noite, {}. Como foi o dia?"};
        std::vector<std::string> madrugada = {"Opa, {}, madrugada produtiva?", "Ainda acordado, {}? Tô aqui."};
        std::vector<std::string> soltas = {
            "Sabia? O 1º bug da computação foi uma mariposa.", "Que tal um gole de água?",
            "Estica as costas, {}. Eu fico de olho.", "Commits pequenos, merges felizes.",
            "Seus agentes estão em boas mãos.", "Pergunta das suas sessões quando quiser.",
            "Dica: três cliques na roda abrem os apps.", "Feito é melhor que perfeito.",
            "Olha para longe da tela por 20 segundos.", "Um passo de cada vez, {}.",
            "Tô de olho em tudo por aqui.", "Precisa de resumo de reunião? Eu gravo.",
        };
#endif
        time_t t = time(nullptr);
        struct tm agora;
        localtime_r(&t, &agora);
        if (agora.tm_year + 1900 >= 2025) {  // relógio já sincronizado
            int h = agora.tm_hour;
            const auto& periodo = (h >= 5 && h < 12) ? manha : (h >= 12 && h < 18) ? tarde : (h >= 18) ? noite : madrugada;
            frases.insert(frases.end(), periodo.begin(), periodo.end());
            if (saudacao) {
                frases = periodo;  // ao entrar na espera: saudação do período
            }
        }
        if (!saudacao) {
            frases.insert(frases.end(), soltas.begin(), soltas.end());
        }
        int i = esp_random() % frases.size();
        if (frases[i] == ultima_frase_) {
            i = (i + 1) % frases.size();
        }
        ultima_frase_ = frases[i];
        std::string f = frases[i];
        auto pos = f.find("{}");
        if (pos == std::string::npos) {
            return f;  // frase sem vocativo
        }
        if (nome.empty()) {  // sem nome: tira o vocativo (", {}" ou "{}, "; em chinês "，{}" ou "{}，")
            const std::string virgulas[] = {", ", "，"};
            size_t ini = pos, fim = pos + 2;
            for (const auto& v : virgulas) {
                if (pos >= v.size() && f.compare(pos - v.size(), v.size(), v) == 0) {
                    ini = pos - v.size();
                    break;
                }
            }
            if (ini == pos) {
                for (const auto& v : virgulas) {
                    if (f.compare(pos + 2, v.size(), v) == 0) {
                        fim = pos + 2 + v.size();
                        break;
                    }
                }
            }
            f.erase(ini, fim - ini);
        } else {
            f.replace(pos, 2, nome);
        }
        return f;
    }
};

// App "Configurações": a roda escolhe o item, o clique troca para a próxima opção (aplica e salva na hora).
// "Cliques na roda" escolhe o que 1, 2 e 3 cliques fazem fora da gaveta (placa/cliques_watcher.h).
// "Atualização" consulta o servidor do OTA e, se houver versão nova, baixa e instala (o aparelho reinicia).
#pragma once

#include <atomic>
#include <vector>

#include <esp_app_desc.h>
#include <esp_system.h>

#include "lvgl_theme.h"
#include "ota.h"
#include "../abertura_watcher.h"
#include "../agente_watcher.h"
#include "../cliques_watcher.h"
#include "../fontes_watcher.h"
#include "../idioma_watcher.h"
#include "../nucleo_apps.h"

class AppConfiguracoes : public AppWatcher {
public:
    const char* Nome() const override { return TR("Ajustes", "Settings", "设置", "Ajustes"); }
    const char* Id() const override { return "ajustes"; }
    const char* Icone() const override { return MATERIAL_SYMBOLS_SETTINGS; }
    std::string Detalhe() const override { return TR("Nome, tema, fonte, tela…", "Name, theme, font, screen…", "名称、主题、字体、屏幕…", "Nombre, tema, fuente…"); }

    void Abrir(ContextoApps& c) override {
        sobre_ = false;
        cliques_ = false;
        ota_ = EstadoOta::Nada;
        Desenhar(c, 0);
    }

    void Girar(ContextoApps& c, int passo) override {
        if (!sobre_ && ota_ != EstadoOta::Buscando) {
            c.painel.Mover(passo);  // na lista, os itens; na versão nova, os botões
        }
    }

    bool Voltar(ContextoApps& c) override {
        if (sobre_) {
            sobre_ = false;
            Desenhar(c, kSobre);
            return true;
        }
        if (ota_ != EstadoOta::Nada) {
            ota_ = EstadoOta::Nada;  // a busca em andamento é descartada (o Fundo não desenha mais)
            Desenhar(c, kAtualizar);
            return true;
        }
        if (cliques_) {
            cliques_ = false;
            Desenhar(c, kCliques);
            return true;
        }
        return false;
    }

    bool Clicar(ContextoApps& c) override {
        if (sobre_) {
            sobre_ = false;
            Desenhar(c, kSobre);
            return true;
        }
        if (ota_ == EstadoOta::Buscando) {
            return true;
        }
        if (cliques_) {
            int gesto = c.painel.Selecionado() + 1;  // linhas: 1, 2 e 3 cliques; depois o Voltar
            if (gesto > CliquesWatcher::kGestos) {
                cliques_ = false;
                Desenhar(c, kCliques);
                return true;
            }
            TrocarAcao(c, gesto);
            return true;
        }
        if (ota_ == EstadoOta::Pronta) {
            if (c.painel.Selecionado() == 0) {  // "Atualizar agora": a tela principal mostra o progresso
                ota_ = EstadoOta::Nada;
                if (c.fechar) {
                    c.fechar();
                }
                // Tarefa própria, como a de ativação do XiaoZhi: no laço principal (Schedule) o download o
                // bloquearia e o progresso ("37% 120KB/s"), que também vai pelo Schedule, só apareceria no fim
                auto* pedido = new std::pair<std::string, std::string>(url_, versao_nova_);
                xTaskCreate(
                    [](void* arg) {
                        auto* p = static_cast<std::pair<std::string, std::string>*>(arg);
                        ContextoApps::App().UpgradeFirmware(p->first, p->second);  // sucesso: reinicia
                        delete p;
                        vTaskDelete(NULL);
                    },
                    "ollie_ota", 4096 * 2, pedido, 2, nullptr);
                return true;
            }
            ota_ = EstadoOta::Nada;
            Desenhar(c, kAtualizar);
            return true;
        }
        if (ota_ == EstadoOta::Resultado) {
            ota_ = EstadoOta::Nada;
            Desenhar(c, kAtualizar);
            return true;
        }
        int i = c.painel.Selecionado();
        switch (i) {
            case kAgente: {
                // Troca o nome; a palavra de ativação só muda ao reiniciar, então reinicia 8 s depois
                // do último clique (dá para girar pela lista de nomes antes)
                AgenteWatcher::Definir(AgenteWatcher::Proximo(AgenteWatcher::Nome()));
                reiniciar_em_ = ContextoApps::Agora() + 8;
                break;
            }
            case kTema: {
                std::string tema = ConfigWatcher::Texto("tema", "dark") == "dark" ? "light" : "dark";
                ConfigWatcher::SetTexto("tema", tema);
                auto* t = LvglThemeManager::GetInstance().GetTheme(tema);
                if (t != nullptr) {
                    Board::GetInstance().GetDisplay()->SetTheme(t);
                }
                break;
            }
            case kFonte: {
                Fontes::DefinirMono(!Fontes::Mono());
                // Reaplicar o tema refaz o layout da fala (sensecap_watcher.cc) já com a fonte nova
                auto* t = LvglThemeManager::GetInstance().GetTheme(ConfigWatcher::Texto("tema", "dark"));
                if (t != nullptr) {
                    Board::GetInstance().GetDisplay()->SetTheme(t);
                }
                break;
            }
            case kTela: {
                int v = Proximo(kOpcoesTela, ConfigWatcher::Int("tela_s", 60));
                ConfigWatcher::SetInt("tela_s", v);
                if (c.definir_tela_apaga_s) {
                    c.definir_tela_apaga_s(v);
                }
                break;
            }
            case kBrilho: {
                int v = Proximo(kOpcoesBrilho, Board::GetInstance().GetBacklight()->brightness());
                Board::GetInstance().GetBacklight()->SetBrightness(v, true);
                break;
            }
            case kVolume: {
                auto codec = Board::GetInstance().GetAudioCodec();
                codec->SetOutputVolume(Proximo(kOpcoesVolume, codec->output_volume()));
                break;
            }
            case kDesliga: {
                int v = Proximo(kOpcoesDesliga, ConfigWatcher::Int("desliga_s", 300));
                ConfigWatcher::SetInt("desliga_s", v);
                if (c.definir_desliga_s) {
                    c.definir_desliga_s(v);
                }
                break;
            }
            case kVoz: {  // respostas faladas: desligadas, o Ollie responde só por texto na tela
                bool ligada = !ConfigWatcher::Int("voz", 1);
                ConfigWatcher::SetInt("voz", ligada ? 1 : 0);
                ContextoApps::App().DefinirVoz(ligada);
                break;
            }
            case kEconomia: {  // brilho 30%, sem "Hey Ollie", pulso e avisos mais espaçados
                bool ligar = !ConfigWatcher::Int("economia", 0);
                ConfigWatcher::SetInt("economia", ligar ? 1 : 0);
                auto luz = Board::GetInstance().GetBacklight();
                if (ligar) {
                    ConfigWatcher::SetInt("brilho_antes", luz->brightness());
                    if (luz->brightness() > kBrilhoEconomia) {
                        luz->SetBrightness(kBrilhoEconomia, true);
                    }
                } else {
                    luz->SetBrightness(ConfigWatcher::Int("brilho_antes", luz->brightness()), true);
                }
                ContextoApps::App().AplicarAtivacao();
                break;
            }
            case kAtivacao: {  // microfone sempre ouvindo a palavra de ativação (desligado: só pela roda)
                ConfigWatcher::SetInt("ativacao", ConfigWatcher::Int("ativacao", 1) ? 0 : 1);
                ContextoApps::App().AplicarAtivacao();
                break;
            }
            case kAvisos:
                ConfigWatcher::SetInt("avisos", ConfigWatcher::Int("avisos", 1) ? 0 : 1);
                break;
            case kCliques:
                cliques_ = true;
                aviso_gaveta_ = 0;
                DesenharCliques(c, 0);
                return true;
            case kAtualizar:
                ota_ = EstadoOta::Buscando;
                c.painel.MostrarStatus(TR("Atualização", "Update", "更新", "Actualización"), PainelWatcher::Status::Carregando,
                                       TR("Procurando atualização…", "Checking for updates…", "正在检查更新…", "Buscando actualizaciones…"),
                                       {}, "searching");
                return true;
            case kSobre:
                sobre_ = true;
                MostrarSobre(c);
                return true;
            default:
                return false;  // Voltar
        }
        Desenhar(c, i);
        return true;
    }

private:
    static constexpr int kBrilhoEconomia = 30;  // % ao ligar a economia de energia
    enum { kAgente, kVoz, kEconomia, kAtivacao, kTema, kFonte, kTela, kBrilho, kVolume, kDesliga, kAvisos, kCliques, kAtualizar, kSobre };
    bool cliques_ = false;  // na tela "Cliques na roda"
    int aviso_gaveta_ = 0;  // gesto que tentou deixar de abrir a gaveta sendo o único
    // Atualização: Buscando (consulta no Fundo), Pronta (versão nova, botões), Resultado (já na mais nova ou erro)
    enum class EstadoOta { Nada, Buscando, Pronta, Resultado };
    std::atomic<EstadoOta> ota_{EstadoOta::Nada};
    std::string url_, versao_nova_;
    inline static const std::vector<int> kOpcoesTela = {30, 60, 120, 300, 900, -1};
    inline static const std::vector<int> kOpcoesBrilho = {25, 50, 75, 100};
    inline static const std::vector<int> kOpcoesVolume = {0, 20, 40, 60, 80, 100};
    inline static const std::vector<int> kOpcoesDesliga = {300, 900, 1800, -1};
    bool sobre_ = false;
    std::atomic<int> reiniciar_em_{0};

    static int Proximo(const std::vector<int>& opcoes, int atual) {
        for (size_t i = 0; i < opcoes.size(); i++) {
            if (opcoes[i] > atual || (opcoes[i] == -1 && atual != -1)) {
                return opcoes[i];
            }
        }
        return opcoes.front();
    }

    static std::string Tempo(int s) {
        if (s < 0) {
            return TR("Nunca", "Never", "永不", "Nunca");
        }
        return s < 60 ? std::to_string(s) + TR(" s", " s", " 秒", " s") : std::to_string(s / 60) + TR(" min", " min", " 分钟", " min");
    }

    void Desenhar(ContextoApps& c, int selecionado) {
        auto& board = Board::GetInstance();
        c.painel.MostrarLista(
            TR("Configurações", "Settings", "设置", "Ajustes"),
            {{TR("Nome do agente", "Agent name", "助手名称", "Nombre del agente"), "Hey " + AgenteWatcher::Nome() +
                                    (reiniciar_em_ ? TR(" · reinicia em instantes", " · restarting soon", " · 即将重启", " · se reinicia pronto") : "")},
             {TR("Respostas faladas", "Spoken replies", "语音回复", "Respuestas habladas"),
              ConfigWatcher::Int("voz", 1) ? TR("Ligadas", "On", "开启", "Activadas")
                                           : TR("Só texto na tela", "Text only", "仅屏幕文字", "Solo texto")},
             {TR("Economia de energia", "Battery saver", "省电模式", "Ahorro de energía"),
              ConfigWatcher::Int("economia", 0) ? TR("Ligada: brilho 30%, sem \"Hey\", avisos a cada 1 min",
                                                     "On: 30% brightness, no \"Hey\", notices every 1 min",
                                                     "开启：亮度 30%，无唤醒词，每 1 分钟通知",
                                                     "Activado: brillo 30%, sin \"Hey\", avisos cada 1 min")
                                                : TR("Desligada", "Off", "关闭", "Desactivado")},
             {TR("Ouvir \"Hey ", "Listen for \"Hey ", "聆听 \"Hey ", "Escuchar \"Hey ") + AgenteWatcher::Nome() + "\"",
              ConfigWatcher::Int("economia", 0) ? TR("Desligado pela economia de energia", "Off (battery saver)", "关闭（省电模式）", "Desactivado (ahorro de energía)")
              : ConfigWatcher::Int("ativacao", 1) ? TR("Ligado (gasta mais bateria)", "On (uses more battery)", "开启（更耗电）", "Activado (gasta más batería)")
                                                  : TR("Desligado: só pela roda", "Off: wheel only", "关闭：仅用滚轮", "Desactivado: solo la rueda")},
             {TR("Tema", "Theme", "主题", "Tema"), ConfigWatcher::Texto("tema", "dark") == "dark" ? TR("Escuro", "Dark", "深色", "Oscuro") : TR("Claro", "Light", "浅色", "Claro")},
             {TR("Fonte", "Font", "字体", "Fuente"), Fontes::Nome()},
             {TR("Tela apaga após", "Screen off after", "熄屏时间", "Apagar pantalla"), Tempo(ConfigWatcher::Int("tela_s", 60))},
             {TR("Brilho", "Brightness", "亮度", "Brillo"), std::to_string(board.GetBacklight()->brightness()) + "%"},
             {TR("Volume", "Volume", "音量", "Volumen"), std::to_string(board.GetAudioCodec()->output_volume()) + "%"},
             {TR("Desliga na bateria após", "Battery off after", "电池关机时间", "Apagar con batería"), Tempo(ConfigWatcher::Int("desliga_s", 300))},
             {TR("Avisos na tela", "On-screen alerts", "屏幕提醒", "Avisos en pantalla"), ConfigWatcher::Int("avisos", 1) ? TR("Ligados", "On", "开启", "Activados") : TR("Desligados", "Off", "关闭", "Desactivados")},
             {TR("Cliques na roda", "Wheel clicks", "滚轮点击", "Clics de la rueda"), ResumoCliques(c)},
             {TR("Atualização", "Update", "更新", "Actualización"),
              TR("Versão ", "Version ", "版本 ", "Versión ") + std::string(esp_app_get_description()->version) +
                  TR(" · procurar nova", " · check for new", " · 检查新版本", " · buscar nueva")},
             {TR("Sobre o Watcher", "About Watcher", "关于 Watcher", "Acerca de Watcher"), TR("Versão, rede e cartão", "Version, network, SD card", "版本、网络和存储卡", "Versión, red y tarjeta")},
             {TR("Voltar", "Back", "返回", "Volver"), ""}},
            selecionado);
    }

    // Nome de uma ação dos cliques, para a lista
    static std::string NomeAcao(ContextoApps& c, const std::string& acao) {
        if (acao == "agente") {
            return TR("Conversar com ", "Talk to ", "和 ", "Hablar con ") + AgenteWatcher::Nome() + TR("", "", " 对话", "");
        }
        if (acao == "gaveta") {
            return TR("Abrir a gaveta", "Open the drawer", "打开应用抽屉", "Abrir el cajón");
        }
        if (acao.rfind("app:", 0) == 0 && c.listar_apps) {
            for (auto& [id, nome] : c.listar_apps()) {
                if (id == acao.substr(4)) {
                    return TR("Abrir ", "Open ", "打开 ", "Abrir ") + nome;
                }
            }
        }
        return TR("Nada", "Nothing", "无", "Nada");
    }

    // Uma linha na lista principal, na ordem 1, 2 e 3 cliques: "Conversar · Gaveta · Nada"
    static std::string ResumoCliques(ContextoApps& c) {
        std::string r;
        for (int n = 1; n <= CliquesWatcher::kGestos; n++) {
            std::string acao = CliquesWatcher::Acao(n);
            std::string curto;
            if (acao == "agente") {
                curto = TR("Conversar", "Talk", "对话", "Hablar");
            } else if (acao == "gaveta") {
                curto = TR("Gaveta", "Drawer", "抽屉", "Cajón");
            } else if (acao == "nada") {
                curto = TR("Nada", "Nothing", "无", "Nada");
            } else {
                curto = NomeAcao(c, acao);
                curto = curto.substr(curto.find(' ') + 1);  // sem o "Abrir"
            }
            r += (n > 1 ? " · " : "") + curto;
        }
        return r;
    }

    void DesenharCliques(ContextoApps& c, int selecionado) {
        static const char* const kNomesGestos[] = {TR("1 clique", "1 click", "单击", "1 clic"), TR("2 cliques", "2 clicks", "双击", "2 clics"),
                                                   TR("3 cliques", "3 clicks", "三击", "3 clics")};
        std::vector<PainelWatcher::Item> itens;
        for (int n = 1; n <= CliquesWatcher::kGestos; n++) {
            std::string detalhe = NomeAcao(c, CliquesWatcher::Acao(n));
            if (aviso_gaveta_ == n) {
                detalhe = TR("Único que abre a gaveta: troque outro antes", "Only one opening the drawer: change another first",
                             "唯一打开抽屉的手势：请先修改其他", "Único que abre el cajón: cambia otro antes");
            }
            itens.push_back({kNomesGestos[n - 1], detalhe});
        }
        itens.push_back({TR("Voltar", "Back", "返回", "Volver"), ""});
        c.painel.MostrarLista(TR("Cliques na roda", "Wheel clicks", "滚轮点击", "Clics de la rueda"), itens, selecionado);
    }

    // Próxima ação do gesto: conversar, gaveta, cada app da gaveta, nada (e de volta ao começo)
    void TrocarAcao(ContextoApps& c, int gesto) {
        std::string atual = CliquesWatcher::Acao(gesto);
        aviso_gaveta_ = 0;
        if (atual == "gaveta" && !CliquesWatcher::OutroAbreGaveta(gesto)) {
            aviso_gaveta_ = gesto;
            DesenharCliques(c, gesto - 1);
            return;
        }
        std::vector<std::string> opcoes = {"agente", "gaveta"};
        if (c.listar_apps) {
            for (auto& [id, nome] : c.listar_apps()) {
                opcoes.push_back("app:" + id);
            }
        }
        opcoes.push_back("nada");
        size_t i = 0;
        while (i < opcoes.size() && opcoes[i] != atual) {
            i++;
        }
        CliquesWatcher::Definir(gesto, opcoes[(i + 1) % opcoes.size()]);  // ação desconhecida: volta ao começo
        DesenharCliques(c, gesto - 1);
    }

    // Consulta ao servidor do OTA (rede: fica fora do clique, que roda com a tela travada)
    void BuscarAtualizacao(ContextoApps& c) {
        ::Ota ota;
        auto resultado = ota.CheckVersion();
        if (ota_ != EstadoOta::Buscando || (c.gaveta_aberta && !c.gaveta_aberta())) {
            return;  // a pessoa voltou ou fechou a gaveta enquanto procurava
        }
        const char* titulo = TR("Atualização", "Update", "更新", "Actualización");
        std::vector<std::string> voltar = {TR("Voltar", "Back", "返回", "Volver")};
        if (!resultado) {
            ota_ = EstadoOta::Resultado;
            c.painel.MostrarStatus(titulo, PainelWatcher::Status::Erro,
                                   TR("Não consegui falar com o servidor. Confira a internet e tente de novo.",
                                      "Could not reach the server. Check the internet and try again.",
                                      "无法连接服务器。请检查网络后重试。",
                                      "No pude hablar con el servidor. Revisa internet e inténtalo de nuevo."),
                                   voltar);
        } else if (ota.HasNewVersion() && !ota.GetFirmwareUrl().empty()) {
            url_ = ota.GetFirmwareUrl();
            versao_nova_ = ota.GetFirmwareVersion();
            ota_ = EstadoOta::Pronta;
            c.painel.MostrarTexto(titulo,
                                  TR("Versão nova: ", "New version: ", "新版本：", "Versión nueva: ") + versao_nova_ +
                                      TR(" (você está na ", " (you have ", "（当前 ", " (tienes la ") + ota.GetCurrentVersion() +
                                      TR(").\n\nO Watcher baixa, instala e reinicia sozinho. Não desligue até terminar.",
                                         ").\n\nThe Watcher downloads, installs and restarts by itself. Don't turn it off until it's done.",
                                         "）。\n\nWatcher 会自动下载、安装并重启。完成前请勿关机。",
                                         ").\n\nEl Watcher descarga, instala y se reinicia solo. No lo apagues hasta que termine."),
                                  {TR("Atualizar agora", "Update now", "立即更新", "Actualizar ahora"),
                                   TR("Agora não", "Not now", "暂不", "Ahora no")});
        } else {
            ota_ = EstadoOta::Resultado;
            c.painel.MostrarStatus(titulo, PainelWatcher::Status::Sucesso,
                                   TR("Você já está na versão mais nova (", "You have the latest version (", "已是最新版本（",
                                      "Ya tienes la versión más nueva (") + ota.GetCurrentVersion() + ").",
                                   voltar);
        }
    }

    // Reinício depois de trocar o nome do agente: avisa o servidor e reinicia para a nova ativação
    void Fundo(ContextoApps& c) override {
        if (ota_ == EstadoOta::Buscando) {
            BuscarAtualizacao(c);
        }
        int quando = reiniciar_em_;
        if (quando == 0 || ContextoApps::Agora() < quando) {
            return;
        }
        reiniciar_em_ = 0;
        std::string corpo;
        RedeWatcher::Pedir("GET", "/watcher/perfil?agente=" + AgenteWatcher::Nome(), "", corpo);
        c.painel.MostrarStatus(TR("Nome do agente", "Agent name", "助手名称", "Nombre del agente"), PainelWatcher::Status::Sucesso,
                               TR("Agora é ", "Now it's ", "现在叫 ", "Ahora es ") + AgenteWatcher::Nome() +
                                   TR(". Diga “Hey ", ". Say “Hey ", "。请说“Hey ", ". Di “Hey ") + AgenteWatcher::Nome() +
                                   TR("”. Reiniciando…", "”. Restarting…", "”。正在重启…", "”. Reiniciando…"));
        vTaskDelay(pdMS_TO_TICKS(2500));
        esp_restart();
    }

    void MostrarSobre(ContextoApps& c) {
        auto& wifi = WifiManager::GetInstance();
        std::string servidor = RedeWatcher::BaseUrl();
        auto pos = servidor.find("://");
        servidor = servidor.substr(pos == std::string::npos ? 0 : pos + 3);
        servidor = servidor.substr(0, servidor.find('/'));
        std::string texto = std::string("Ollie v") + OllieVersao() +
                            TR(" (base XiaoZhi ", " (based on XiaoZhi ", " (基于 XiaoZhi ", " (base XiaoZhi ") +
                            OLLIE_BASE_XIAOZHI +
                            ")\nWi-Fi: " + (wifi.IsConnected() ? wifi.GetSsid() : std::string(TR("desconectado", "disconnected", "未连接", "desconectado"))) +
                            "\nIP: " + (wifi.IsConnected() ? wifi.GetIpAddress() : std::string("-")) +
                            "\nMAC: " + SystemInfo::GetMacAddress() + TR("\nServidor: ", "\nServer: ", "\n服务器：", "\nServidor: ") + servidor +
                            "\nmicroSD: " + (CartaoWatcher::Instancia().Montado() ? TR("montado", "mounted", "已挂载", "montada") : TR("ausente", "not inserted", "未插入", "no insertada"));
        c.painel.MostrarTexto(TR("Sobre o Watcher", "About Watcher", "关于 Watcher", "Acerca de Watcher"), texto, {TR("Voltar", "Back", "返回", "Volver")});
    }
};

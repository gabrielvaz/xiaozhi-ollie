// Núcleo da plataforma de apps da gaveta do SenseCAP Watcher.
//
// Um app é uma classe que herda AppWatcher. A gaveta chama:
//   Abrir()   quando o usuário escolhe o app (desenhe a primeira tela);
//   Girar()   a cada passo da roda (+1 horário, -1 anti-horário);
//   Clicar()  a cada clique; devolva false para voltar à gaveta;
//   Voltar()  dois cliques; devolva true se voltou a uma tela anterior do próprio app;
//   Tique()   a cada 500 ms enquanto o app está aberto (pode usar rede e cartão);
//   Fundo()   a cada 500 ms sempre, mesmo fechado (cronômetro correndo, avisos, sincronização).
// Girar/Clicar rodam na tarefa dos botões: só mudem estado e a tela; trabalho lento
// (rede, cartão) vai para Tique/Fundo, que rodam na tarefa da gaveta.
// Para registrar um app novo: uma linha em registro_apps.h.
#pragma once

#include <atomic>

#include <cJSON.h>
#include <esp_timer.h>

#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "application.h"
#include "board.h"
#include "cartao_watcher.h"
#include "diagnostico_watcher.h"
#include "painel_watcher.h"
#include "settings.h"
#include "system_info.h"
#include "wifi_manager.h"

// ------------------------------------------------------------------ configurações persistentes

class ConfigWatcher {
public:
    static int Int(const char* chave, int padrao) {
        Settings s("watcher", false);
        return s.GetInt(chave, padrao);
    }
    static void SetInt(const char* chave, int valor) {
        Settings s("watcher", true);
        s.SetInt(chave, valor);
    }
    static std::string Texto(const char* chave, const std::string& padrao) {
        Settings s("watcher", false);
        std::string v = s.GetString(chave);
        return v.empty() ? padrao : v;
    }
    static void SetTexto(const char* chave, const std::string& valor) {
        Settings s("watcher", true);
        s.SetString(chave, valor);
    }
};

// ------------------------------------------------------------------ rede com o servidor do Mac

class RedeWatcher {
public:
    static bool Online() { return WifiManager::GetInstance().IsConnected(); }

    static std::string BaseUrl() {
        Settings wifi("wifi", false);
        std::string url = wifi.GetString("ota_url");
        if (url.empty()) {
            url = CONFIG_OTA_URL;
        }
        auto pos = url.find("/xiaozhi/ota");
        return pos == std::string::npos ? url : url.substr(0, pos);
    }

    // GET/POST autenticado no servidor do Mac (mesmo token do canal de voz)
    static bool Pedir(const char* metodo, const std::string& caminho, const std::string& conteudo, std::string& corpo) {
        if (!Online()) {
            return false;
        }
        auto& board = Board::GetInstance();
        auto rede = board.GetNetwork();
        if (rede == nullptr) {
            return false;
        }
        Settings ws("websocket", false);
        std::string token = ws.GetString("token");
        if (token.empty()) {
            return false;
        }
        auto http = rede->CreateHttp(0);
        // 15 s (o padrão era 30 s, e a gaveta inteira esperava); áudio gerado na hora pode demorar mais
        http->SetTimeout(caminho.find("/audio") != std::string::npos ? 90000 : 15000);
        DiagnosticoWatcher::Marcar("%s %.32s", metodo, caminho.c_str());
        http->SetHeader("Device-Id", SystemInfo::GetMacAddress().c_str());
        http->SetHeader("Client-Id", board.GetUuid());
        http->SetHeader("Authorization", "Bearer " + token);
        if (!conteudo.empty() || std::string(metodo) == "POST") {
            http->SetHeader("Content-Type", "application/octet-stream");
            http->SetContent(std::string(conteudo));
        }
        if (auto aberto = http->Open(metodo, BaseUrl() + caminho); !aberto) {
            DiagnosticoWatcher::Marcar("  falhou ao abrir");
            return false;
        }
        auto status = http->GetStatusCode();
        if (!status || *status != 200) {
            DiagnosticoWatcher::Marcar("  status %d", status ? *status : -1);
            http->Close();
            return false;
        }
        corpo = http->ReadAll();
        http->Close();
        DiagnosticoWatcher::Marcar("  ok %u bytes", (unsigned)corpo.size());
        return true;
    }

    // Igual a Pedir, mas reaproveita a resposta guardada se tiver até idade_max_s segundos: cada conexão
    // segura nova custa 1 a 3 s no aparelho, então reabrir uma tela fica instantâneo. Só GET.
    static bool PedirCache(const std::string& caminho, int idade_max_s, std::string& corpo) {
        static std::map<std::string, std::pair<int, std::string>> guardados;
        int agora = (int)(esp_timer_get_time() / 1000000);
        auto it = guardados.find(caminho);
        if (it != guardados.end() && agora - it->second.first <= idade_max_s) {
            corpo = it->second.second;
            return true;
        }
        if (!Pedir("GET", caminho, "", corpo)) {
            return false;
        }
        if (guardados.size() > 24) {
            guardados.clear();
        }
        guardados[caminho] = {agora, corpo};
        return true;
    }

    static std::string Campo(cJSON* obj, const char* chave) {
        cJSON* v = obj ? cJSON_GetObjectItem(obj, chave) : nullptr;
        return cJSON_IsString(v) ? v->valuestring : "";
    }
    static int Numero(cJSON* obj, const char* chave, int padrao = -1) {
        cJSON* v = obj ? cJSON_GetObjectItem(obj, chave) : nullptr;
        return cJSON_IsNumber(v) ? v->valueint : padrao;
    }
};

// ------------------------------------------------------------------ contexto entregue aos apps

struct ContextoApps {
    PainelWatcher& painel;
    std::function<void()> voltar_gaveta;  // fecha o app e mostra a gaveta
    std::function<void()> fechar;         // fecha a gaveta inteira
    // Ajustes que dependem da placa (preenchidos pela placa)
    std::function<void(int)> definir_tela_apaga_s;
    std::function<void(int)> definir_desliga_s;
    std::function<bool()> gaveta_aberta;  // serviços evitam mexer na tela com a gaveta aberta
    // Abre a gaveta direto num app (pelo Id()) com um argumento, ex.: um aviso abrindo uma sessão
    std::function<void(const std::string& app, const std::string& argumento)> abrir_app;
    // Apps da gaveta como (Id, Nome), na ordem do mosaico (Configurações > Cliques na roda)
    std::function<std::vector<std::pair<std::string, std::string>>()> listar_apps;

    static int Agora() { return (int)(esp_timer_get_time() / 1000000); }
    static Application& App() { return Application::GetInstance(); }

    // Pede algo ao Ollie (assistente) como se o usuário tivesse falado (fecha a gaveta antes)
    void Perguntar(const std::string& frase) {
        fechar();
        App().WakeWordInvoke(frase);
    }

    // Aviso na tela principal (som opcional: Lang::Sounds::OGG_POPUP etc.)
    inline static std::atomic<int> ultimo_aviso{-100000};
    // Sessões do Claude Code trabalhando agora (vem junto com os avisos; a tela de espera mostra o Clawd trabalhando)
    inline static std::atomic<int> atividade_n{0};
    inline static std::string atividade_titulo;  // escrito e lido só pela tarefa da gaveta
    inline static int atividade_inicio = -1;
    inline static std::atomic<bool> tela_apagada{false};  // a placa marca ao apagar/acender (economia de bateria)     // Agora() em que a 1ª sessão começou o turno (-1: não se sabe)
    // Lista do app Claude Code que veio junto com a última consulta de avisos (abre sem esperar a rede)
    inline static std::string sessoes_json;
    inline static int sessoes_quando = -100000;  // Agora() do último aviso (a saudação não o cobre)

    static void Avisar(const std::string& titulo, const std::string& texto, const std::string& emocao,
                       std::string_view som = "") {
        ultimo_aviso = Agora();
        App().Schedule([titulo, texto, emocao, som]() {
            App().Alert(titulo.c_str(), texto.c_str(), emocao.c_str(), som);
        });
    }
};

// ------------------------------------------------------------------ interface dos apps

class AppWatcher {
public:
    virtual ~AppWatcher() = default;
    virtual const char* Nome() const = 0;
    virtual const char* Id() const { return Nome(); }    // identificador fixo (não traduzido) para abrir_app
    virtual std::string Detalhe() const { return ""; }   // linha de baixo na gaveta
    virtual const char* Icone() const { return nullptr; } // MATERIAL_SYMBOLS_* (material_symbols.h)
    virtual bool Visivel() const { return true; }        // false = só serviço de fundo
    virtual void Abrir(ContextoApps& c) = 0;
    virtual void AbrirCom(ContextoApps& c, const std::string& argumento) { Abrir(c); }  // aberto por outro app/aviso
    virtual void Girar(ContextoApps& c, int passo) {}
    virtual bool Clicar(ContextoApps& c) = 0;            // false = volta para a gaveta
    virtual bool Voltar(ContextoApps& c) { return false; }  // dois cliques; true = tratou (tela anterior do app)
    virtual void Tique(ContextoApps& c) {}
    virtual void Fundo(ContextoApps& c) {}
    virtual bool PrendeTela() const { return false; }    // true = 3 cliques não fecham (ex.: gravação)
};

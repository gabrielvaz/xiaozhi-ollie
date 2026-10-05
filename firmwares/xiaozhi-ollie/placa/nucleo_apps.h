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
#include <string>

#include "application.h"
#include "board.h"
#include "cartao_watcher.h"
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
        http->SetHeader("Device-Id", SystemInfo::GetMacAddress().c_str());
        http->SetHeader("Client-Id", board.GetUuid());
        http->SetHeader("Authorization", "Bearer " + token);
        if (!conteudo.empty() || std::string(metodo) == "POST") {
            http->SetHeader("Content-Type", "application/octet-stream");
            http->SetContent(std::string(conteudo));
        }
        if (auto aberto = http->Open(metodo, BaseUrl() + caminho); !aberto) {
            return false;
        }
        auto status = http->GetStatusCode();
        if (!status || *status != 200) {
            http->Close();
            return false;
        }
        corpo = http->ReadAll();
        http->Close();
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

    static int Agora() { return (int)(esp_timer_get_time() / 1000000); }
    static Application& App() { return Application::GetInstance(); }

    // Pede algo ao Ollie (assistente) como se o usuário tivesse falado (fecha a gaveta antes)
    void Perguntar(const std::string& frase) {
        fechar();
        App().WakeWordInvoke(frase);
    }

    // Aviso na tela principal (som opcional: Lang::Sounds::OGG_POPUP etc.)
    inline static std::atomic<int> ultimo_aviso{-100000};  // Agora() do último aviso (a saudação não o cobre)

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
    virtual std::string Detalhe() const { return ""; }   // linha de baixo na gaveta
    virtual const char* Icone() const { return nullptr; } // MATERIAL_SYMBOLS_* (material_symbols.h)
    virtual bool Visivel() const { return true; }        // false = só serviço de fundo
    virtual void Abrir(ContextoApps& c) = 0;
    virtual void Girar(ContextoApps& c, int passo) {}
    virtual bool Clicar(ContextoApps& c) = 0;            // false = volta para a gaveta
    virtual bool Voltar(ContextoApps& c) { return false; }  // dois cliques; true = tratou (tela anterior do app)
    virtual void Tique(ContextoApps& c) {}
    virtual void Fundo(ContextoApps& c) {}
    virtual bool PrendeTela() const { return false; }    // true = 3 cliques não fecham (ex.: gravação)
};

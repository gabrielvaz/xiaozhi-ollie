// App "Mostrar QR code": o primeiro é o Wi-Fi atual (montado no aparelho); os demais vêm de
// iCloud Drive/Watcher/QR.md no Mac ("- Título | conteúdo"), guardados para funcionar offline.
// Girar troca o QR, clique volta.
#pragma once

#include <atomic>
#include <vector>

#include "../idioma_watcher.h"
#include "../nucleo_apps.h"
#include "ssid_manager.h"

class AppQrCode : public AppWatcher {
public:
    const char* Nome() const override { return TR("QR code", "QR code", "二维码", "Código QR"); }
    const char* Icone() const override { return MATERIAL_SYMBOLS_LINK; }
    std::string Detalhe() const override { return TR("Wi-Fi, links e contatos", "Wi-Fi, links and contacts", "Wi-Fi、链接和联系人", "Wi-Fi, enlaces y contactos"); }

    void Abrir(ContextoApps& c) override {
        MontarLista();
        atual_ = 0;
        Desenhar(c);
        buscar_ = true;  // atualiza a lista do Mac em segundo plano
    }

    void Girar(ContextoApps& c, int passo) override {
        int n = itens_.size();
        if (n == 0) {
            return;
        }
        atual_ = ((atual_ + passo) % n + n) % n;
        Desenhar(c);
    }

    bool Clicar(ContextoApps& c) override { return false; }

    void Tique(ContextoApps& c) override {
        if (!buscar_.exchange(false)) {
            return;
        }
        std::string corpo;
        if (RedeWatcher::Pedir("GET", "/watcher/qrcodes", "", corpo)) {
            if (corpo.size() < 3500) {  // guarda na memória do aparelho para funcionar offline
                ConfigWatcher::SetTexto("qrcodes", corpo);
            }
            int antes = itens_.size();
            MontarLista();
            if ((int)itens_.size() != antes) {
                Desenhar(c);
            }
        }
    }

private:
    struct Item {
        std::string titulo, conteudo, legenda;
    };
    std::vector<Item> itens_;
    int atual_ = 0;
    std::atomic<bool> buscar_{false};

    // Escapa os caracteres especiais do formato WIFI: do QR
    static std::string Escapar(const std::string& s) {
        std::string r;
        for (char ch : s) {
            if (ch == '\\' || ch == ';' || ch == ',' || ch == ':' || ch == '"') {
                r += '\\';
            }
            r += ch;
        }
        return r;
    }

    void MontarLista() {
        itens_.clear();
        auto& wifi = WifiManager::GetInstance();
        std::string ssid = wifi.IsConnected() ? wifi.GetSsid() : "";
        for (const auto& rede : SsidManager::GetInstance().GetSsidList()) {
            if (rede.ssid == ssid) {
                itens_.push_back({"Wi-Fi " + ssid,
                                  "WIFI:T:" + std::string(rede.password.empty() ? "nopass" : "WPA") + ";S:" + Escapar(rede.ssid) +
                                      ";P:" + Escapar(rede.password) + ";;",
                                  TR("aponte a câmera do celular para entrar", "point your phone camera to join",
                                     "用手机相机扫码连接", "apunta la cámara del móvil para entrar")});
            }
        }
        cJSON* raiz = cJSON_Parse(ConfigWatcher::Texto("qrcodes", "{}").c_str());
        cJSON* lista = raiz ? cJSON_GetObjectItem(raiz, "itens") : nullptr;
        cJSON* item = nullptr;
        cJSON_ArrayForEach(item, lista) {
            std::string texto = RedeWatcher::Campo(item, "texto");
            itens_.push_back({RedeWatcher::Campo(item, "titulo"), texto, texto});
        }
        cJSON_Delete(raiz);
    }

    void Desenhar(ContextoApps& c) {
        if (itens_.empty()) {
            c.painel.MostrarTexto(TR("Mostrar QR code", "Show QR code", "显示二维码", "Mostrar código QR"),
                                  TR("Nenhum QR ainda. Edite iCloud Drive/Watcher/QR.md no Mac.",
                                     "No QR codes yet. Edit iCloud Drive/Watcher/QR.md on the Mac.",
                                     "还没有二维码。请在 Mac 上编辑 iCloud Drive/Watcher/QR.md。",
                                     "Aún no hay códigos QR. Edita iCloud Drive/Watcher/QR.md en el Mac."),
                                  {TR("Voltar", "Back", "返回", "Volver")});
            return;
        }
        const auto& q = itens_[atual_];
        std::string legenda = q.legenda;
        if (itens_.size() > 1) {
            legenda = std::to_string(atual_ + 1) + "/" + std::to_string(itens_.size()) + " · " + legenda;
        }
        c.painel.MostrarQr(q.titulo, q.conteudo, legenda);
    }
};

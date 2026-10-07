// App "Wi-Fi por QR": a câmera do Watcher lê o QR code de uma rede (o que o iPhone e o Android mostram ao
// compartilhar o Wi-Fi), o app confirma o nome, salva a rede e reinicia para conectar nela.
// No modo de configuração de Wi-Fi, 1 clique na roda abre este app já lendo (sensecap_watcher.cc).
#pragma once

#include <atomic>
#include <string>

#include <esp_system.h>
#include <ssid_manager.h>

#include "../idioma_watcher.h"
#include "../leitor_qr.h"
#include "../nucleo_apps.h"
#include "../sscma_camera.h"  // a câmera da placa (apps/ fica dentro da pasta da placa)

class AppWifiQr : public AppWatcher {
public:
    const char* Nome() const override { return TR("Wi-Fi por QR", "Wi-Fi from QR", "扫码连 Wi-Fi", "Wi-Fi por QR"); }
    const char* Id() const override { return "wifi_qr"; }
    const char* Icone() const override { return MATERIAL_SYMBOLS_WIFI; }
    std::string Detalhe() const override {
        return TR("Lê o QR da rede com a câmera", "Reads the network QR with the camera", "用相机扫描网络二维码",
                  "Lee el QR de la red con la cámara");
    }

    void Abrir(ContextoApps& c) override {
        estado_ = Estado::Inicio;
        c.painel.MostrarTexto(Nome(),
                              TR("Mostre à câmera o QR code da rede Wi-Fi, a uns 10 a 15 cm.\n\n"
                                 "iPhone: app Senhas > Wi-Fi > a rede > Mostrar QR Code.\n"
                                 "Android: Wi-Fi > a rede > Compartilhar.",
                                 "Show the Wi-Fi network's QR code to the camera, about 10 to 15 cm away.\n\n"
                                 "iPhone: Passwords app > Wi-Fi > the network > Show Network QR Code.\n"
                                 "Android: Wi-Fi > the network > Share.",
                                 "把 Wi-Fi 网络的二维码对准相机，距离约 10 到 15 厘米。\n\n"
                                 "iPhone：密码 App > Wi-Fi > 网络 > 显示网络二维码。\nAndroid：WLAN > 网络 > 分享。",
                                 "Muestra a la cámara el QR de la red Wi-Fi, a unos 10 a 15 cm.\n\n"
                                 "iPhone: app Contraseñas > Wi-Fi > la red > Mostrar código QR.\n"
                                 "Android: Wi-Fi > la red > Compartir."),
                              {TR("Ler QR code", "Read QR code", "扫描二维码", "Leer QR"), TR("Voltar", "Back", "返回", "Volver")});
    }

    // Aberto pelo modo de configuração de Wi-Fi (1 clique): vai direto para a leitura
    void AbrirCom(ContextoApps& c, const std::string& argumento) override {
        if (argumento == "ler") {
            Ler(c);
        } else {
            Abrir(c);
        }
    }

    void Girar(ContextoApps& c, int passo) override {
        if (estado_ != Estado::Lendo) {
            c.painel.Mover(passo);  // botões
        }
    }

    bool Voltar(ContextoApps& c) override {
        if (estado_ == Estado::Inicio) {
            return false;
        }
        Abrir(c);
        return true;
    }

    bool Clicar(ContextoApps& c) override {
        int botao = c.painel.Selecionado();
        switch (estado_.load()) {
            case Estado::Inicio:
                if (botao == 0) {
                    Ler(c);
                    return true;
                }
                return false;  // Voltar
            case Estado::Achou:
                if (botao == 0) {
                    Conectar(c);
                    return true;
                }
                Abrir(c);
                return true;
            case Estado::Salvo:
                return true;  // reiniciando
            default:  // lendo (Cancelar) ou erro (Voltar)
                Abrir(c);
                return true;
        }
    }

    // Lê na tarefa da gaveta: cada tentativa tira uma foto (~1,5 s) e procura o QR
    void Fundo(ContextoApps& c) override {
        if (estado_ == Estado::Salvo && ContextoApps::Agora() >= reiniciar_em_) {
            esp_restart();
        }
        if (estado_ != Estado::Lendo || (c.gaveta_aberta && !c.gaveta_aberta())) {
            return;
        }
        if (ContextoApps::Agora() - inicio_ > kTempoMaximoS) {
            estado_ = Estado::Erro;
            c.painel.MostrarStatus(Nome(), PainelWatcher::Status::Erro,
                                   TR("Não achei um QR code. Aproxime o celular (uns 10 a 15 cm), com o brilho alto, e tente de novo.",
                                      "No QR code found. Hold the phone closer (about 10 to 15 cm), screen bright, and try again.",
                                      "没有找到二维码。请把手机靠近（约 10 到 15 厘米），调亮屏幕后重试。",
                                      "No encontré un QR. Acerca el móvil (unos 10 a 15 cm), con el brillo alto, e inténtalo de nuevo."),
                                   {TR("Voltar", "Back", "返回", "Volver")});
            return;
        }
        auto camera = static_cast<SscmaCamera*>(Board::GetInstance().GetCamera());
        if (camera == nullptr || !camera->Capture()) {
            return;  // tenta de novo no próximo ciclo
        }
        int w = 0, h = 0;
        const uint16_t* imagem = camera->UltimaImagemRgb(w, h);
        if (imagem == nullptr || estado_ != Estado::Lendo) {
            return;
        }
        std::string texto = LeitorQr::Ler(imagem, w, h);
        if (texto.empty()) {
            return;
        }
        auto rede = LeitorQr::InterpretarWifi(texto);
        if (!rede.valida) {
            c.painel.MostrarStatus(Nome(), PainelWatcher::Status::Carregando,
                                   TR("Esse QR não é de Wi-Fi. Mostre o QR da rede…", "That QR isn't a Wi-Fi one. Show the network's QR…",
                                      "这不是 Wi-Fi 二维码。请出示网络的二维码…", "Ese QR no es de Wi-Fi. Muestra el QR de la red…"),
                                   {TR("Cancelar", "Cancel", "取消", "Cancelar")}, "confused");
            return;
        }
        rede_ = rede;
        estado_ = Estado::Achou;
        c.painel.MostrarTexto(Nome(),
                              TR("Rede: ", "Network: ", "网络：", "Red: ") + rede_.ssid + "\n" +
                                  (rede_.senha.empty() ? TR("Sem senha", "No password", "无密码", "Sin contraseña")
                                                       : TR("Com senha", "With password", "有密码", "Con contraseña")) +
                                  TR("\n\nO Watcher salva a rede e reinicia para conectar nela.",
                                     "\n\nThe Watcher saves the network and restarts to connect to it.",
                                     "\n\nWatcher 会保存该网络并重启以连接。",
                                     "\n\nEl Watcher guarda la red y se reinicia para conectarse."),
                              {TR("Conectar", "Connect", "连接", "Conectar"), TR("Cancelar", "Cancel", "取消", "Cancelar")});
    }

private:
    enum class Estado { Inicio, Lendo, Achou, Salvo, Erro };
    static constexpr int kTempoMaximoS = 45;
    std::atomic<Estado> estado_{Estado::Inicio};
    int inicio_ = 0;
    int reiniciar_em_ = 0;
    LeitorQr::RedeWifi rede_;

    void Ler(ContextoApps& c) {
        inicio_ = ContextoApps::Agora();
        estado_ = Estado::Lendo;
        c.painel.MostrarStatus(Nome(), PainelWatcher::Status::Carregando,
                               TR("Aponte a câmera para o QR code…", "Point the camera at the QR code…", "请把相机对准二维码…",
                                  "Apunta la cámara al código QR…"),
                               {TR("Cancelar", "Cancel", "取消", "Cancelar")}, "searching");
    }

    // Salva a rede (vai para o topo da lista) e reinicia: no boot ele procura as redes salvas e conecta
    void Conectar(ContextoApps& c) {
        SsidManager::GetInstance().AddSsid(rede_.ssid, rede_.senha);
        estado_ = Estado::Salvo;
        reiniciar_em_ = ContextoApps::Agora() + 3;
        c.painel.MostrarStatus(Nome(), PainelWatcher::Status::Sucesso,
                               TR("Rede ", "Network ", "网络 ", "Red ") + rede_.ssid +
                                   TR(" salva. Reiniciando para conectar…", " saved. Restarting to connect…", " 已保存。正在重启以连接…",
                                      " guardada. Reiniciando para conectar…"));
    }
};

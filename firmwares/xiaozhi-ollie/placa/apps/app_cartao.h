// App "Enviar cartão ao Mac": conversas e reuniões do microSD vão para iCloud Drive/Watcher/Do cartão.
// Também copia as conversas do Mac para o iCloud (POST /watcher/backup): é o único momento em que isso acontece.
#pragma once

#include <atomic>
#include <ctime>

#include "../idioma_watcher.h"
#include "../nucleo_apps.h"

class AppCartao : public AppWatcher {
public:
    const char* Nome() const override { return TR("Backup", "Backup", "备份", "Copia de seguridad"); }
    const char* Icone() const override { return MATERIAL_SYMBOLS_SD_CARD; }
    std::string Detalhe() const override { return TR("Conversas e reuniões do microSD", "Chats and meetings on microSD", "microSD 上的对话和会议", "Chats y reuniones de la microSD"); }

    void Abrir(ContextoApps& c) override {
        c.painel.MostrarStatus(TR("Fazer backup", "Back up", "备份", "Copia de seguridad"), PainelWatcher::Status::Carregando, TR("Fazendo backup no Mac…", "Backing up to the Mac…", "正在备份到 Mac…", "Copiando al Mac…"));
        enviar_ = true;
    }

    bool Clicar(ContextoApps& c) override { return enviando_; }  // depois de enviar, clique volta

    void Tique(ContextoApps& c) override {
        if (!enviar_.exchange(false)) {
            return;
        }
        enviando_ = true;
        int n = Enviar(true);
        if (n >= 0) {  // conversas do Mac para o iCloud (só acontece aqui, no backup)
            std::string resposta;
            RedeWatcher::Pedir("POST", "/watcher/backup", "{}", resposta);
        }
        enviando_ = false;
        if (n < 0) {
            c.painel.MostrarStatus(TR("Fazer backup", "Back up", "备份", "Copia de seguridad"), PainelWatcher::Status::Erro, TR("Sem microSD ou sem conexão agora.", "No microSD or no connection right now.", "没有 microSD 卡或暂时无法连接。", "Sin microSD o sin conexión ahora."), {TR("Voltar", "Back", "返回", "Volver")});
        } else {
            c.painel.MostrarStatus(TR("Fazer backup", "Back up", "备份", "Copia de seguridad"), PainelWatcher::Status::Sucesso,
                                   n == 0 ? TR("Tudo já estava no Mac.", "Everything was already on the Mac.", "所有内容都已在 Mac 上。", "Todo ya estaba en el Mac.") : std::to_string(n) + TR(" arquivo(s) enviado(s) ao Mac.", " file(s) sent to the Mac.", " 个文件已发送到 Mac。", " archivo(s) enviado(s) al Mac."),
                                   {TR("Voltar", "Back", "返回", "Volver")});
        }
    }

private:
    static constexpr size_t kParte = 32 * 1024;
    std::atomic<bool> enviar_{false};
    std::atomic<bool> enviando_{false};

    static bool EnviarArquivo(const std::string& tipo, const std::string& pasta, const std::string& nome) {
        std::string caminho = pasta + nome;
        long tamanho = CartaoWatcher::Tamanho(caminho);
        if (tamanho < 0) {
            return false;
        }
        long offset = 0;
        do {
            std::string parte, resposta;
            CartaoWatcher::LerParte(caminho, offset, kParte, parte);
            bool fim = offset + (long)parte.size() >= tamanho;
            std::string url = "/watcher/upload?tipo=" + tipo + "&nome=" + nome + "&offset=" + std::to_string(offset) +
                              (fim ? "&fim=1" : "");
            if (!RedeWatcher::Pedir("POST", url, parte, resposta)) {
                return false;
            }
            offset += (long)parte.size();
            if (parte.empty()) {
                break;
            }
        } while (offset < tamanho);
        return true;
    }

    // Reuniões pendentes sempre; conversas só no envio manual (as de dias anteriores vão para "enviados")
    static int Enviar(bool incluir_conversas) {
        auto& cartao = CartaoWatcher::Instancia();
        if (!cartao.Montado() || !RedeWatcher::Online()) {
            return -1;
        }
        cartao.Descarregar();
        int enviados = 0;
        std::string reunioes = std::string(CartaoWatcher::kPasta) + "/reunioes/";
        for (const auto& nome : CartaoWatcher::Listar(reunioes)) {
            if (EnviarArquivo("reunioes", reunioes, nome)) {
                CartaoWatcher::Mover(reunioes + nome, "/enviados/");
                enviados++;
            }
        }
        if (incluir_conversas) {
            time_t agora = time(nullptr);
            struct tm t;
            localtime_r(&agora, &t);
            char hoje[24];
            strftime(hoje, sizeof(hoje), "%Y-%m-%d.txt", &t);
            std::string conversas = std::string(CartaoWatcher::kPasta) + "/conversas/";
            for (const auto& nome : CartaoWatcher::Listar(conversas)) {
                if (EnviarArquivo("conversas", conversas, nome)) {
                    if (nome != hoje) {
                        CartaoWatcher::Mover(conversas + nome, "/enviados/");
                    }
                    enviados++;
                }
            }
        }
        return enviados;
    }
};

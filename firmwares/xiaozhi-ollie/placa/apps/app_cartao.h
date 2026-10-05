// App "Enviar cartão ao Mac": conversas e reuniões do microSD vão para iCloud Drive/Watcher/Do cartão.
// Em segundo plano, reuniões que ficaram só no cartão (queda de conexão) são enviadas a cada hora.
#pragma once

#include <atomic>
#include <ctime>

#include "../nucleo_apps.h"

class AppCartao : public AppWatcher {
public:
    const char* Nome() const override { return "Backup"; }
    const char* Icone() const override { return MATERIAL_SYMBOLS_SD_CARD; }
    std::string Detalhe() const override { return "Conversas e reuniões do microSD"; }

    void Abrir(ContextoApps& c) override {
        c.painel.MostrarStatus("Fazer backup", PainelWatcher::Status::Carregando, "Fazendo backup no Mac…");
        enviar_ = true;
    }

    bool Clicar(ContextoApps& c) override { return enviando_; }  // depois de enviar, clique volta

    void Tique(ContextoApps& c) override {
        if (!enviar_.exchange(false)) {
            return;
        }
        enviando_ = true;
        int n = Enviar(true);
        enviando_ = false;
        if (n < 0) {
            c.painel.MostrarStatus("Fazer backup", PainelWatcher::Status::Erro, "Sem microSD ou sem conexão agora.", {"Voltar"});
        } else {
            c.painel.MostrarStatus("Fazer backup", PainelWatcher::Status::Sucesso,
                                   n == 0 ? "Tudo já estava no Mac." : std::to_string(n) + " arquivo(s) enviado(s) ao Mac.",
                                   {"Voltar"});
        }
    }

    void Fundo(ContextoApps& c) override {
        int agora = ContextoApps::Agora();
        if (agora - ultimo_envio_ < 3600 || ContextoApps::App().GetDeviceState() != kDeviceStateIdle) {
            return;
        }
        ultimo_envio_ = agora;
        Enviar(false);
    }

private:
    static constexpr size_t kParte = 32 * 1024;
    std::atomic<bool> enviar_{false};
    std::atomic<bool> enviando_{false};
    int ultimo_envio_ = 0;

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

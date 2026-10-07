// App "Backup": conversas e reuniões do microSD vão para iCloud Drive/Watcher/Do cartão.
// Abrir mostra o que está pendente e pede confirmação; o envio tem progresso ("3/12") e Cancelar.
// Também copia as conversas do Mac para o iCloud (POST /watcher/backup): é o único momento em que isso acontece.
#pragma once

#include <atomic>
#include <ctime>

#include "../idioma_watcher.h"
#include "../nucleo_apps.h"

class AppCartao : public AppWatcher {
public:
    const char* Nome() const override { return TR("Backup", "Backup", "备份", "Copia de seguridad"); }
    const char* Id() const override { return "backup"; }
    const char* Icone() const override { return MATERIAL_SYMBOLS_SD_CARD; }
    std::string Detalhe() const override { return TR("Conversas e reuniões do microSD", "Chats and meetings on microSD", "microSD 上的对话和会议", "Chats y reuniones de la microSD"); }

    void Abrir(ContextoApps& c) override {
        tela_ = Tela::Conferindo;
        c.painel.MostrarStatus(Titulo(), PainelWatcher::Status::Carregando,
                               TR("Conferindo o cartão…", "Checking the card…", "正在检查存储卡…", "Revisando la tarjeta…"));
        pedido_ = Pedido::Conferir;
    }

    void Girar(ContextoApps& c, int passo) override {
        if (tela_ == Tela::Confirmar || tela_ == Tela::Resultado) {
            c.painel.Mover(passo);
        }
    }

    bool Clicar(ContextoApps& c) override {
        switch (tela_.load()) {
            case Tela::Conferindo:
                return true;
            case Tela::Confirmar:
                if (c.painel.Selecionado() == 0) {  // Enviar
                    tela_ = Tela::Enviando;
                    cancelar_ = false;
                    c.painel.MostrarValor(Titulo(), "0/" + std::to_string(std::max(1, pendentes_)),
                                          TR("preparando…", "preparing…", "正在准备…", "preparando…"),
                                          {TR("Cancelar", "Cancel", "取消", "Cancelar")});
                    pedido_ = Pedido::Enviar;
                    return true;
                }
                return false;  // Voltar
            case Tela::Enviando:
                cancelar_ = true;  // o envio para no próximo pedaço
                return true;
            case Tela::Resultado:
                return false;  // Voltar
        }
        return false;
    }

    bool Voltar(ContextoApps& c) override {
        if (tela_ == Tela::Enviando) {
            cancelar_ = true;
            return true;
        }
        return false;
    }

    void Tique(ContextoApps& c) override {
        Pedido p = pedido_.exchange(Pedido::Nada);
        if (p == Pedido::Conferir) {
            Conferir(c);
        } else if (p == Pedido::Enviar) {
            Enviar(c);
        }
    }

private:
    enum class Tela { Conferindo, Confirmar, Enviando, Resultado };
    enum class Pedido { Nada, Conferir, Enviar };
    static constexpr size_t kParte = 32 * 1024;
    std::atomic<Tela> tela_{Tela::Conferindo};
    std::atomic<Pedido> pedido_{Pedido::Nada};
    std::atomic<bool> cancelar_{false};
    int pendentes_ = 0;

    static const char* Titulo() { return TR("Backup", "Backup", "备份", "Copia de seguridad"); }

    static std::string PastaReunioes() { return std::string(CartaoWatcher::kPasta) + "/reunioes/"; }
    static std::string PastaConversas() { return std::string(CartaoWatcher::kPasta) + "/conversas/"; }

    // O que está esperando no cartão; mostra e pede confirmação antes de mexer em qualquer coisa
    void Conferir(ContextoApps& c) {
        auto& cartao = CartaoWatcher::Instancia();
        if (!cartao.Montado() && !RedeWatcher::Online()) {
            tela_ = Tela::Resultado;
            c.painel.MostrarStatus(Titulo(), PainelWatcher::Status::Erro,
                                   TR("Sem microSD e sem conexão com o Mac agora.", "No microSD and no connection to the Mac right now.",
                                      "没有 microSD 卡，也暂时连不上 Mac。", "Sin microSD y sin conexión con el Mac ahora."),
                                   {TR("Voltar", "Back", "返回", "Volver")});
            return;
        }
        int reunioes = 0, conversas = 0;
        if (cartao.Montado()) {
            cartao.Descarregar();
            reunioes = (int)CartaoWatcher::Listar(PastaReunioes()).size();
            conversas = (int)CartaoWatcher::Listar(PastaConversas()).size();
        }
        pendentes_ = reunioes + conversas;
        tela_ = Tela::Confirmar;
        std::string texto;
        if (pendentes_ > 0) {
            char linha[160];
            snprintf(linha, sizeof(linha),
                     TR("No cartão: %d reunião(ões) e %d conversa(s) para enviar ao Mac.",
                        "On the card: %d meeting(s) and %d chat log(s) to send to the Mac.",
                        "卡上有 %d 段会议录音和 %d 份对话记录待发送到 Mac。",
                        "En la tarjeta: %d reunión(es) y %d conversación(es) para enviar al Mac."),
                     reunioes, conversas);
            texto = linha;
        } else {
            texto = cartao.Montado()
                        ? TR("Nada pendente no cartão.", "Nothing pending on the card.", "卡上没有待发送的内容。", "Nada pendiente en la tarjeta.")
                        : TR("Sem microSD.", "No microSD.", "没有 microSD 卡。", "Sin microSD.");
        }
        texto += TR("\n\nO backup também copia as conversas do Mac para o iCloud Drive.",
                    "\n\nThe backup also copies the Mac's chats to iCloud Drive.",
                    "\n\n备份还会把 Mac 上的对话复制到 iCloud 云盘。",
                    "\n\nLa copia también lleva los chats del Mac a iCloud Drive.");
        c.painel.MostrarTexto(Titulo(), texto,
                              {TR("Fazer backup", "Back up", "开始备份", "Hacer copia"), TR("Voltar", "Back", "返回", "Volver")});
    }

    // Um arquivo em pedaços de 32 KB; retorna false se falhou ou se a pessoa cancelou
    bool EnviarArquivo(const std::string& tipo, const std::string& pasta, const std::string& nome) {
        std::string caminho = pasta + nome;
        long tamanho = CartaoWatcher::Tamanho(caminho);
        if (tamanho < 0) {
            return false;
        }
        long offset = 0;
        do {
            if (cancelar_) {
                return false;
            }
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

    void Progresso(ContextoApps& c, int feito, int total, const std::string& nome) {
        c.painel.AtualizarValor(std::to_string(feito) + "/" + std::to_string(total),
                                nome.empty() ? std::string(" ") : nome);
    }

    void Enviar(ContextoApps& c) {
        auto& cartao = CartaoWatcher::Instancia();
        int enviados = 0, falhas = 0;
        bool online = RedeWatcher::Online();
        if (cartao.Montado() && online) {
            cartao.Descarregar();
            std::string reunioes = PastaReunioes();
            auto nomes_reunioes = CartaoWatcher::Listar(reunioes);
            std::string conversas = PastaConversas();
            auto nomes_conversas = CartaoWatcher::Listar(conversas);
            int total = (int)(nomes_reunioes.size() + nomes_conversas.size());
            for (const auto& nome : nomes_reunioes) {
                if (cancelar_) {
                    break;
                }
                Progresso(c, enviados + falhas, total, nome);
                if (EnviarArquivo("reunioes", reunioes, nome)) {
                    CartaoWatcher::Mover(reunioes + nome, "/enviados/");
                    enviados++;
                } else {
                    falhas++;
                }
            }
            time_t agora = time(nullptr);
            struct tm t;
            localtime_r(&agora, &t);
            char hoje[24];
            strftime(hoje, sizeof(hoje), "%Y-%m-%d.txt", &t);
            for (const auto& nome : nomes_conversas) {
                if (cancelar_) {
                    break;
                }
                Progresso(c, enviados + falhas, total, nome);
                if (EnviarArquivo("conversas", conversas, nome)) {
                    if (nome != hoje) {  // a conversa de hoje ainda cresce: fica no cartão
                        CartaoWatcher::Mover(conversas + nome, "/enviados/");
                    }
                    enviados++;
                } else {
                    falhas++;
                }
            }
        }
        // Conversas do Mac para o iCloud (só acontece aqui, no backup)
        bool mac_ok = false;
        if (online && !cancelar_) {
            std::string resposta;
            mac_ok = RedeWatcher::Pedir("POST", "/watcher/backup", "{}", resposta);
        }
        if (c.gaveta_aberta && !c.gaveta_aberta()) {
            return;  // a gaveta foi fechada no meio: não desenha um resultado por cima da tela principal
        }
        tela_ = Tela::Resultado;
        if (cancelar_) {
            c.painel.MostrarStatus(Titulo(), PainelWatcher::Status::Erro,
                                   TR("Backup interrompido: ", "Backup stopped: ", "备份已中断：", "Copia interrumpida: ") +
                                       std::to_string(enviados) +
                                       TR(" arquivo(s) enviado(s). O resto fica para a próxima.",
                                          " file(s) sent. The rest waits for next time.",
                                          " 个文件已发送，其余留到下次。",
                                          " archivo(s) enviado(s). El resto queda para la próxima."),
                                   {TR("Voltar", "Back", "返回", "Volver")});
            return;
        }
        if (!online) {
            c.painel.MostrarStatus(Titulo(), PainelWatcher::Status::Erro,
                                   TR("Sem conexão com o Mac agora.", "No connection to the Mac right now.",
                                      "暂时连不上 Mac。", "Sin conexión con el Mac ahora."),
                                   {TR("Voltar", "Back", "返回", "Volver")});
            return;
        }
        std::string texto;
        if (enviados == 0 && falhas == 0) {
            texto = TR("Tudo já estava no Mac.", "Everything was already on the Mac.", "所有内容都已在 Mac 上。", "Todo ya estaba en el Mac.");
        } else {
            texto = std::to_string(enviados) + TR(" arquivo(s) enviado(s) ao Mac.", " file(s) sent to the Mac.",
                                                  " 个文件已发送到 Mac。", " archivo(s) enviado(s) al Mac.");
            if (falhas > 0) {
                texto += " " + std::to_string(falhas) + TR(" falhou(aram): tente de novo.", " failed: try again.",
                                                           " 个失败：请重试。", " con error: inténtalo de nuevo.");
            }
        }
        if (mac_ok) {
            texto += TR(" Conversas do Mac copiadas para o iCloud.", " The Mac's chats were copied to iCloud.",
                        " Mac 上的对话已复制到 iCloud。", " Los chats del Mac se copiaron a iCloud.");
        }
        c.painel.MostrarStatus(Titulo(), falhas == 0 ? PainelWatcher::Status::Sucesso : PainelWatcher::Status::Erro,
                               texto, {TR("Voltar", "Back", "返回", "Volver")});
    }
};

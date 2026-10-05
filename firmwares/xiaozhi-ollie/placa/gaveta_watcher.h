// Gaveta de apps do SenseCAP Watcher: 3 cliques abrem/fecham, a roda navega, o clique escolhe.
// Os apps ficam em apps/ e são registrados em registro_apps.h.
#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

#include "nucleo_apps.h"

class GavetaWatcher {
public:
    explicit GavetaWatcher(Display* display)
        : painel_(display),
          contexto_{painel_, [this]() { VoltarGaveta(); }, [this]() { FecharTudo(); }, nullptr, nullptr,
                    [this]() { return aberta_.load(); }} {}

    ContextoApps& Contexto() { return contexto_; }

    void Registrar(std::unique_ptr<AppWatcher> app) { apps_.push_back(std::move(app)); }

    void Iniciar() {
        xTaskCreate([](void* arg) { static_cast<GavetaWatcher*>(arg)->Laco(); }, "gaveta_watcher", 12288, this, 3, nullptr);
    }

    // Três cliques
    void AlternarGaveta() {
        std::lock_guard<std::recursive_mutex> trava(trava_);
        if (ativo_ != nullptr && ativo_->PrendeTela()) {
            return;
        }
        if (aberta_) {
            FecharTudo();
        } else {
            VoltarGaveta();
        }
    }

    // Dois cliques: com a gaveta fechada, abre; com ela aberta, volta uma tela
    void AbrirOuVoltar() {
        std::lock_guard<std::recursive_mutex> trava(trava_);
        if (!aberta_) {
            VoltarGaveta();
        } else {
            Voltar();
        }
    }

    // Três cliques: fecha a gaveta de qualquer tela (menos as que prendem a tela, como a gravação)
    void Fechar() {
        std::lock_guard<std::recursive_mutex> trava(trava_);
        if (aberta_ && !(ativo_ != nullptr && ativo_->PrendeTela())) {
            FecharTudo();
        }
    }

    // Dois cliques: volta uma tela (dentro do app, para a gaveta, ou fecha a gaveta)
    void Voltar() {
        std::lock_guard<std::recursive_mutex> trava(trava_);
        if (!aberta_ || (ativo_ != nullptr && ativo_->PrendeTela())) {
            return;
        }
        if (ativo_ == nullptr) {
            FecharTudo();
        } else if (!ativo_->Voltar(contexto_)) {
            VoltarGaveta();
        }
    }

    // Retorna true se a roda foi usada pela gaveta (senão ela controla o volume)
    bool Girar(bool horario) {
        std::lock_guard<std::recursive_mutex> trava(trava_);
        if (!aberta_) {
            return false;
        }
        int passo = horario ? 1 : -1;
        if (ativo_ != nullptr) {
            ativo_->Girar(contexto_, passo);
        } else {
            painel_.Mover(passo);
        }
        return true;
    }

    // Retorna true se o clique foi usado pela gaveta (senão ele inicia a conversa)
    bool Clicar() {
        std::lock_guard<std::recursive_mutex> trava(trava_);
        if (!aberta_) {
            return false;
        }
        if (ativo_ != nullptr) {
            if (!ativo_->Clicar(contexto_) && aberta_) {
                VoltarGaveta();
            }
            return true;
        }
        int i = painel_.Selecionado();
        auto visiveis = Visiveis();
        if (i < 0 || i >= (int)visiveis.size()) {  // "Fechar"
            FecharTudo();
            return true;
        }
        ativo_ = visiveis[i];
        ultimo_indice_ = i;
        ativo_->Abrir(contexto_);
        return true;
    }

private:
    PainelWatcher painel_;
    ContextoApps contexto_;
    std::vector<std::unique_ptr<AppWatcher>> apps_;
    AppWatcher* ativo_ = nullptr;
    std::atomic<bool> aberta_{false};
    int ultimo_indice_ = 0;
    std::recursive_mutex trava_;

    std::vector<AppWatcher*> Visiveis() {
        std::vector<AppWatcher*> v;
        for (auto& a : apps_) {
            if (a->Visivel()) {
                v.push_back(a.get());
            }
        }
        return v;
    }

    void VoltarGaveta() {
        std::lock_guard<std::recursive_mutex> trava(trava_);
        ativo_ = nullptr;
        aberta_ = true;
        std::vector<PainelWatcher::Item> itens;
        for (auto* a : Visiveis()) {
            itens.push_back({a->Nome(), a->Detalhe(), a->Icone()});
        }
        itens.push_back({"Fechar", "", MATERIAL_SYMBOLS_CLOSE});
        painel_.MostrarGrade("Apps", itens, ultimo_indice_);
    }

    void FecharTudo() {
        std::lock_guard<std::recursive_mutex> trava(trava_);
        ativo_ = nullptr;
        aberta_ = false;
        painel_.Fechar();
    }

    void Laco() {
        while (true) {
            vTaskDelay(pdMS_TO_TICKS(500));
            AppWatcher* ativo;
            {
                // Só lê o app ativo sob a trava: rede e cartão rodam fora dela para a roda não travar
                std::lock_guard<std::recursive_mutex> trava(trava_);
                ativo = ativo_;
            }
            if (ativo != nullptr) {
                ativo->Tique(contexto_);
            }
            for (auto& a : apps_) {
                a->Fundo(contexto_);
            }
        }
    }
};

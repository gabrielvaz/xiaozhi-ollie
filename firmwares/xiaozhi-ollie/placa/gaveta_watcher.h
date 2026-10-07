// Gaveta de apps do SenseCAP Watcher: 3 cliques abrem/fecham, a roda navega, o clique escolhe.
// Os apps ficam em apps/ e são registrados em registro_apps.h.
#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

#include "idioma_watcher.h"
#include "diagnostico_watcher.h"
#include "nucleo_apps.h"

class GavetaWatcher {
public:
    explicit GavetaWatcher(Display* display)
        : painel_(display),
          contexto_{painel_, [this]() { VoltarGaveta(); }, [this]() { FecharTudo(); }, nullptr, nullptr,
                    [this]() { return aberta_.load(); }} {
        contexto_.abrir_app = [this](const std::string& nome, const std::string& argumento) { AbrirApp(nome, argumento); };
        contexto_.listar_apps = [this]() {
            std::vector<std::pair<std::string, std::string>> lista;
            for (auto* a : Visiveis()) {
                lista.emplace_back(a->Id(), a->Nome());
            }
            return lista;
        };
    }

    ContextoApps& Contexto() { return contexto_; }
    bool Aberta() const { return aberta_; }

    // Abre a gaveta direto num app (ex.: aviso de sessão esperando você -> app Sessões)
    void AbrirApp(const std::string& nome, const std::string& argumento) {
        std::lock_guard<std::recursive_mutex> trava(trava_);
        if (ativo_ != nullptr && ativo_->PrendeTela()) {
            return;
        }
        auto visiveis = Visiveis();
        for (int i = 0; i < (int)visiveis.size(); i++) {
            if (nome == visiveis[i]->Id()) {
                aberta_ = true;
                ativo_ = visiveis[i];
                ultimo_indice_ = i + 1;  // índice no mosaico (o 0 é o Voltar)
                DiagnosticoWatcher::Marcar("abre %s (%s)", ativo_->Id(), argumento.substr(0, 12).c_str());
                ativo_->AbrirCom(contexto_, argumento);
                return;
            }
        }
    }

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
        if (i <= 0 || i > (int)visiveis.size()) {  // 0 = "Voltar" (fecha a gaveta)
            FecharTudo();
            return true;
        }
        ativo_ = visiveis[i - 1];
        ultimo_indice_ = i;
        DiagnosticoWatcher::Marcar("abre %s", ativo_->Id());
        ativo_->Abrir(contexto_);
        return true;
    }

private:
    PainelWatcher painel_;
    ContextoApps contexto_;
    std::vector<std::unique_ptr<AppWatcher>> apps_;
    AppWatcher* ativo_ = nullptr;
    std::atomic<bool> aberta_{false};
    int ultimo_indice_ = 1;  // abre no primeiro app (o 0 é o Voltar)
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
        std::vector<PainelWatcher::Item> itens = {{TR("Voltar", "Back", "返回", "Volver"), "", MATERIAL_SYMBOLS_ARROW_BACK}};  // primeira célula
        for (auto* a : Visiveis()) {
            itens.push_back({a->Nome(), a->Detalhe(), a->Icone()});
        }
        painel_.MostrarGrade(TR("Apps", "Apps", "应用", "Apps"), itens, ultimo_indice_);
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
            DiagnosticoWatcher::Batida("laco");
            if (ativo != nullptr) {
                DiagnosticoWatcher::Batida(ativo->Id());  // se travar aqui, o vigia anota qual app
                ativo->Tique(contexto_);
            }
            for (auto& a : apps_) {
                DiagnosticoWatcher::Batida(a->Id());
                a->Fundo(contexto_);
            }
        }
    }
};

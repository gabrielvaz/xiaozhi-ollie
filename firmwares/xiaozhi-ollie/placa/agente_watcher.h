// Nome do agente (Configurações) e do usuário (vem do servidor). O nome muda a palavra de ativação
// ("Hey <nome>", lida em custom_wake_word.cc ao iniciar) e como o agente se apresenta (servidor).
#pragma once

#include <string>

#include "settings.h"

class AgenteWatcher {
public:
    static constexpr const char* kNomes[] = {"Ollie", "Clawd", "Jarvis", "Nova", "Atlas", "Luna", "Max", "Iris"};

    static std::string Nome() {
        Settings s("watcher", false);
        std::string n = s.GetString("agente");
        return n.empty() ? "Ollie" : n;
    }
    static void Definir(const std::string& nome) {
        Settings s("watcher", true);
        s.SetString("agente", nome);
    }
    static std::string Proximo(const std::string& atual) {
        constexpr int n = sizeof(kNomes) / sizeof(kNomes[0]);
        for (int i = 0; i < n; i++) {
            if (atual == kNomes[i]) {
                return kNomes[(i + 1) % n];
            }
        }
        return kNomes[0];
    }

    static std::string Usuario() {
        Settings s("watcher", false);
        return s.GetString("usuario");
    }
    static void DefinirUsuario(const std::string& nome) {
        Settings s("watcher", true);
        s.SetString("usuario", nome);
    }
};

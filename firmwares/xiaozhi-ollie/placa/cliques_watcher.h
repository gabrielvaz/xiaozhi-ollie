// Cliques na roda fora da gaveta (Configurações > Cliques na roda): 1, 2 e 3 cliques fazem a ação escolhida:
// "agente" (conversar), "gaveta" (abrir a gaveta), "app:<Id>" (abrir um app da gaveta direto) ou "nada".
// Dentro da gaveta os cliques são fixos (1 escolhe, 2 voltam, 3 fecham), e algum gesto sempre abre a gaveta,
// porque é por ela que se chega às Configurações.
#pragma once

#include <cstdio>
#include <string>

#include "nucleo_apps.h"

class CliquesWatcher {
public:
    static constexpr int kGestos = 3;

    static std::string Acao(int cliques) {
        static const char* const kPadrao[kGestos] = {"agente", "gaveta", "nada"};  // como era antes
        if (cliques < 1 || cliques > kGestos) {
            return "nada";
        }
        return ConfigWatcher::Texto(Chave(cliques).c_str(), kPadrao[cliques - 1]);
    }

    static void Definir(int cliques, const std::string& acao) { ConfigWatcher::SetTexto(Chave(cliques).c_str(), acao); }

    // Outro gesto (que não este) também abre a gaveta? Senão este não pode deixar de abrir
    static bool OutroAbreGaveta(int cliques) {
        for (int n = 1; n <= kGestos; n++) {
            if (n != cliques && Acao(n) == "gaveta") {
                return true;
            }
        }
        return false;
    }

private:
    static std::string Chave(int cliques) {
        char chave[12];
        snprintf(chave, sizeof(chave), "clique%d", cliques);
        return chave;
    }
};

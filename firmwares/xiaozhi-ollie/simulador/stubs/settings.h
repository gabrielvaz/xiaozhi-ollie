#pragma once
// Simulador: Configurações em memória; a fonte vem da variável de ambiente OLLIE_FONTE (noto ou mono)
#include <cstdlib>
#include <string>

class Settings {
public:
    Settings(const std::string&, bool) {}
    std::string GetString(const std::string& chave, const std::string& padrao = "") {
        const char* v = chave == "fonte" ? std::getenv("OLLIE_FONTE") : nullptr;
        return v ? v : padrao;
    }
    void SetString(const std::string&, const std::string&) {}
    int GetInt(const std::string&, int padrao = 0) { return padrao; }
};

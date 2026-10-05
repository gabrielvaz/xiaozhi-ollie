// App "Relógio mundial": girar troca a cidade (uma por tela), clique volta. Lembra a última cidade.
// O relógio do aparelho guarda a hora de Brasília como se fosse UTC (o servidor ajusta assim),
// então o UTC real = hora do aparelho + 3 h. Horário de verão: regras dos EUA, da Europa e da Austrália.
#pragma once

#include <ctime>
#include <vector>

#include "../nucleo_apps.h"

class AppRelogioMundial : public AppWatcher {
public:
    const char* Nome() const override { return "Relógios"; }
    const char* Icone() const override { return MATERIAL_SYMBOLS_LANGUAGE; }
    std::string Detalhe() const override { return Cidades()[Indice()].nome; }

    void Abrir(ContextoApps& c) override {
        atual_ = Indice();
        Desenhar(c);
    }

    void Girar(ContextoApps& c, int passo) override {
        int n = Cidades().size();
        atual_ = ((atual_ + passo) % n + n) % n;
        ConfigWatcher::SetInt("relogio_cidade", atual_);
        Desenhar(c);
    }

    bool Clicar(ContextoApps& c) override { return false; }

    void Tique(ContextoApps& c) override {
        if (++tiques_ % 10 == 0) {  // a cada 5 s
            Desenhar(c);
        }
    }

private:
    enum class Verao { Nenhum, EUA, Europa, Australia };
    struct Cidade {
        const char* nome;
        int deslocamento_min;  // em relação ao UTC, fora do horário de verão
        Verao verao;
    };

    int atual_ = 0;
    int tiques_ = 0;

    static const std::vector<Cidade>& Cidades() {
        static const std::vector<Cidade> c = {
            {"São Paulo", -180, Verao::Nenhum},     {"Milão", 60, Verao::Europa},
            {"Lisboa", 0, Verao::Europa},           {"Londres", 0, Verao::Europa},
            {"Nova York", -300, Verao::EUA},        {"Miami", -300, Verao::EUA},
            {"San Francisco", -480, Verao::EUA},    {"Dubai", 240, Verao::Nenhum},
            {"Xangai", 480, Verao::Nenhum},         {"Tóquio", 540, Verao::Nenhum},
            {"Sydney", 600, Verao::Australia},
        };
        return c;
    }

    static int Indice() {
        int i = ConfigWatcher::Int("relogio_cidade", 1);
        return (i >= 0 && i < (int)Cidades().size()) ? i : 0;
    }

    // Dia do mês do n-ésimo domingo (n = -1: último) de um mês (0 = jan)
    static int Domingo(int ano, int mes, int n) {
        struct tm t = {};
        t.tm_year = ano - 1900;
        t.tm_mon = mes;
        t.tm_mday = 1;
        t.tm_hour = 12;
        time_t base = timegm(&t);
        gmtime_r(&base, &t);
        int primeiro = 1 + (7 - t.tm_wday) % 7;
        if (n > 0) {
            return primeiro + 7 * (n - 1);
        }
        int ultimo = primeiro;
        static const int dias[] = {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
        while (ultimo + 7 <= dias[mes]) {
            ultimo += 7;
        }
        return ultimo;
    }

    static time_t Utc(int ano, int mes, int dia, int hora) {
        struct tm t = {};
        t.tm_year = ano - 1900;
        t.tm_mon = mes;
        t.tm_mday = dia;
        t.tm_hour = hora;
        return timegm(&t);
    }

    static bool EmVerao(const Cidade& c, time_t utc) {
        struct tm t;
        gmtime_r(&utc, &t);
        int ano = t.tm_year + 1900;
        switch (c.verao) {
            case Verao::EUA:  // 2º domingo de março 2h local até 1º domingo de novembro 2h local
                return utc >= Utc(ano, 2, Domingo(ano, 2, 2), 2) - c.deslocamento_min * 60 &&
                       utc < Utc(ano, 10, Domingo(ano, 10, 1), 1) - c.deslocamento_min * 60;
            case Verao::Europa:  // último domingo de março 1h UTC até último domingo de outubro 1h UTC
                return utc >= Utc(ano, 2, Domingo(ano, 2, -1), 1) && utc < Utc(ano, 9, Domingo(ano, 9, -1), 1);
            case Verao::Australia:  // 1º domingo de outubro até 1º domingo de abril (hemisfério sul)
                return !(utc >= Utc(ano, 3, Domingo(ano, 3, 1), 3) - c.deslocamento_min * 60 - 3600 &&
                         utc < Utc(ano, 9, Domingo(ano, 9, 1), 2) - c.deslocamento_min * 60);
            default:
                return false;
        }
    }

    void Desenhar(ContextoApps& c) {
        static const char* dias[] = {"domingo", "segunda", "terça", "quarta", "quinta", "sexta", "sábado"};
        const auto& cidade = Cidades()[atual_];
        time_t utc = time(nullptr) + 3 * 3600;  // relógio do aparelho = Brasília
        int deslocamento = cidade.deslocamento_min + (EmVerao(cidade, utc) ? 60 : 0);
        time_t local = utc + deslocamento * 60;
        struct tm t;
        gmtime_r(&local, &t);
        char hora[16], legenda[128];
        snprintf(hora, sizeof(hora), "%02d:%02d", t.tm_hour, t.tm_min);
        int dif = deslocamento + 180;  // diferença para Brasília, em minutos
        char dif_txt[48];
        if (dif == 0) {
            snprintf(dif_txt, sizeof(dif_txt), "mesma hora de Brasília");
        } else {
            snprintf(dif_txt, sizeof(dif_txt), "%+d h de Brasília", dif / 60);
        }
        snprintf(legenda, sizeof(legenda), "%s, %02d/%02d\n%s", dias[t.tm_wday], t.tm_mday, t.tm_mon + 1, dif_txt);
        c.painel.MostrarValor(cidade.nome, hora, legenda, {});
    }
};

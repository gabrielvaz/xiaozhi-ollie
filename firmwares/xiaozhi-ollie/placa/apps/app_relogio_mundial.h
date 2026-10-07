// App "Relógio mundial": 24 bolinhas no aro, uma por fuso de hora cheia (UTC+0 no topo, leste no sentido
// horário); girar anda o cursor um fuso, clique volta. Cada fuso tem uma cidade, e a hora mostrada é a dela
// (com horário de verão). Lembra o último fuso.
// O relógio do aparelho guarda a hora local do Mac como se fosse UTC (o servidor ajusta assim); o fuso do
// Mac vem no perfil (utc_off_min, guardado pela saudação) e diz onde fica a "casa" no aro.
// Horário de verão: regras dos EUA, da Europa, da Austrália e da Nova Zelândia.
#pragma once

#include <ctime>
#include <vector>

#include "../idioma_watcher.h"
#include "../nucleo_apps.h"

class AppRelogioMundial : public AppWatcher {
public:
    const char* Nome() const override { return TR("Relógios", "World clock", "世界时钟", "Relojes"); }
    const char* Id() const override { return "relogios"; }
    const char* Icone() const override { return MATERIAL_SYMBOLS_LANGUAGE; }
    std::string Detalhe() const override { return Cidades()[Indice()].nome; }

    void Abrir(ContextoApps& c) override {
        atual_ = Indice();
        Desenhar(c);
    }

    void Girar(ContextoApps& c, int passo) override {
        int n = Cidades().size();
        atual_ = ((atual_ + passo) % n + n) % n;
        salvar_em_ = ContextoApps::Agora() + 2;  // grava na memória 2 s depois do último giro (não a cada passo)
        Desenhar(c);
    }

    bool Clicar(ContextoApps& c) override {
        SalvarSePreciso(true);  // saindo do app: guarda o fuso escolhido
        return false;
    }

    void Tique(ContextoApps& c) override {
        SalvarSePreciso(false);
        if (++tiques_ % 10 == 0) {  // a cada 5 s: com a tela aberta, MostrarAro só troca os textos
            Desenhar(c);
        }
    }

    void Fundo(ContextoApps& c) override { SalvarSePreciso(false); }

private:
    enum class Verao { Nenhum, EUA, Europa, Australia, NovaZelandia };
    struct Cidade {
        const char* nome;
        int deslocamento_min;  // em relação ao UTC, fora do horário de verão
        Verao verao;
    };

    int atual_ = 0;
    int tiques_ = 0;
    int salvar_em_ = 0;  // Agora() em que o fuso escolhido vai para a memória (0 = nada a gravar)

    void SalvarSePreciso(bool agora_mesmo) {
        if (salvar_em_ != 0 && (agora_mesmo || ContextoApps::Agora() >= salvar_em_)) {
            salvar_em_ = 0;
            if (ConfigWatcher::Int("relogio_fuso", kPadrao) != atual_) {
                ConfigWatcher::SetInt("relogio_fuso", atual_);
            }
        }
    }

    // Fuso do Mac (minutos em relação ao UTC), trazido pelo perfil; sem ele, Brasília (UTC-3)
    static int CasaMin() { return ConfigWatcher::Int("utc_off_min", -180); }

    // Posição no aro do fuso de casa (hora cheia mais próxima)
    static int CasaPosicao() {
        int h = (CasaMin() >= 0 ? CasaMin() + 30 : CasaMin() - 30) / 60;  // arredonda para a hora cheia
        return ((h % kPosicoes) + kPosicoes) % kPosicoes;
    }

    // Posição i do aro = UTC+i (i <= 12) ou UTC+i-24 (i > 12); o deslocamento é o da hora padrão
    static const std::vector<Cidade>& Cidades() {
        static const std::vector<Cidade> c = {
            {TR("Londres", "London", "伦敦", "Londres"), 0, Verao::Europa},
            {TR("Milão", "Milan", "米兰", "Milán"), 60, Verao::Europa},
            {TR("Joanesburgo", "Johannesburg", "约翰内斯堡", "Johannesburgo"), 120, Verao::Nenhum},
            {TR("Moscou", "Moscow", "莫斯科", "Moscú"), 180, Verao::Nenhum},
            {TR("Dubai", "Dubai", "迪拜", "Dubái"), 240, Verao::Nenhum},
            {TR("Karachi", "Karachi", "卡拉奇", "Karachi"), 300, Verao::Nenhum},
            {TR("Daca", "Dhaka", "达卡", "Daca"), 360, Verao::Nenhum},
            {TR("Bangkok", "Bangkok", "曼谷", "Bangkok"), 420, Verao::Nenhum},
            {TR("Xangai", "Shanghai", "上海", "Shanghái"), 480, Verao::Nenhum},
            {TR("Tóquio", "Tokyo", "东京", "Tokio"), 540, Verao::Nenhum},
            {TR("Sydney", "Sydney", "悉尼", "Sídney"), 600, Verao::Australia},
            {TR("Nouméa", "Nouméa", "努美阿", "Numea"), 660, Verao::Nenhum},
            {TR("Auckland", "Auckland", "奥克兰", "Auckland"), 720, Verao::NovaZelandia},
            {TR("Pago Pago", "Pago Pago", "帕果帕果", "Pago Pago"), -660, Verao::Nenhum},
            {TR("Honolulu", "Honolulu", "檀香山", "Honolulu"), -600, Verao::Nenhum},
            {TR("Anchorage", "Anchorage", "安克雷奇", "Anchorage"), -540, Verao::EUA},
            {TR("San Francisco", "San Francisco", "旧金山", "San Francisco"), -480, Verao::EUA},
            {TR("Denver", "Denver", "丹佛", "Denver"), -420, Verao::EUA},
            {TR("Cidade do México", "Mexico City", "墨西哥城", "Ciudad de México"), -360, Verao::Nenhum},
            {TR("Nova York", "New York", "纽约", "Nueva York"), -300, Verao::EUA},
            {TR("Manaus", "Manaus", "马瑙斯", "Manaos"), -240, Verao::Nenhum},
            {TR("São Paulo", "São Paulo", "圣保罗", "São Paulo"), -180, Verao::Nenhum},
            {TR("Fernando de Noronha", "Fernando de Noronha", "费尔南多-迪诺罗尼亚", "Fernando de Noronha"), -120, Verao::Nenhum},
            {TR("Praia", "Praia", "普拉亚", "Praia"), -60, Verao::Nenhum},
        };
        return c;
    }

    static constexpr int kPosicoes = 24;
    static constexpr int kPadrao = 21;  // posição de São Paulo (UTC-3), o padrão sem perfil

    static int Indice() {
        int i = ConfigWatcher::Int("relogio_fuso", CasaPosicao());
        return (i >= 0 && i < (int)Cidades().size()) ? i : CasaPosicao();
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
            case Verao::NovaZelandia:  // último domingo de setembro 2h até 1º domingo de abril 3h (hora local)
                return !(utc >= Utc(ano, 3, Domingo(ano, 3, 1), 3) - c.deslocamento_min * 60 - 3600 &&
                         utc < Utc(ano, 8, Domingo(ano, 8, -1), 2) - c.deslocamento_min * 60);
            default:
                return false;
        }
    }

    void Desenhar(ContextoApps& c) {
        static const char* dias[] = {
            TR("domingo", "Sunday", "周日", "domingo"),   TR("segunda", "Monday", "周一", "lunes"),
            TR("terça", "Tuesday", "周二", "martes"),     TR("quarta", "Wednesday", "周三", "miércoles"),
            TR("quinta", "Thursday", "周四", "jueves"),   TR("sexta", "Friday", "周五", "viernes"),
            TR("sábado", "Saturday", "周六", "sábado"),
        };
        const auto& cidade = Cidades()[atual_];
        time_t utc = time(nullptr) - CasaMin() * 60;  // relógio do aparelho = hora local do Mac
        int deslocamento = cidade.deslocamento_min + (EmVerao(cidade, utc) ? 60 : 0);
        time_t local = utc + deslocamento * 60;
        struct tm t;
        gmtime_r(&local, &t);
        char hora[16], legenda[160];
        snprintf(hora, sizeof(hora), "%02d:%02d", t.tm_hour, t.tm_min);
        char utc_txt[16];
        if (deslocamento == 0) {
            snprintf(utc_txt, sizeof(utc_txt), "UTC");
        } else {
            snprintf(utc_txt, sizeof(utc_txt), "UTC%+d", deslocamento / 60);
        }
        int dif = deslocamento - CasaMin();  // diferença para a sua hora, em minutos
        char dif_txt[48];
        if (dif == 0) {
            snprintf(dif_txt, sizeof(dif_txt), TR("a sua hora", "your local time", "你的本地时间", "tu hora local"));
        } else {
            snprintf(dif_txt, sizeof(dif_txt),
                     TR("%+d h de você", "%+d h from you", "与你相差 %+d 小时", "%+d h respecto a ti"),
                     dif / 60);
        }
        // data no formato de cada idioma: dd/mm (pt, es), mm/dd (en), m月d日 (zh)
        char data[24];
        snprintf(data, sizeof(data), TR("%02d/%02d", "%02d/%02d", "%d月%d日", "%02d/%02d"),
                 TR(t.tm_mday, t.tm_mon + 1, t.tm_mon + 1, t.tm_mday), TR(t.tm_mon + 1, t.tm_mday, t.tm_mday, t.tm_mon + 1));
        snprintf(legenda, sizeof(legenda), TR("%s · %s\n%s, %s", "%s · %s\n%s, %s", "%s · %s\n%s %s", "%s · %s\n%s, %s"),
                 utc_txt, dif_txt, TR(dias[t.tm_wday], dias[t.tm_wday], data, dias[t.tm_wday]),
                 TR(data, data, dias[t.tm_wday], data));
        c.painel.MostrarAro(atual_, CasaPosicao(), cidade.nome, hora, legenda);
    }
};

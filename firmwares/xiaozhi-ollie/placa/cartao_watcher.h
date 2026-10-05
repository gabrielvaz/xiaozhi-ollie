// microSD do SenseCAP Watcher: registro local das conversas, cópia das reuniões (.wopus)
// e memória offline baixada do Mac. Pastas em /sdcard/watcher/.
#pragma once

#include <dirent.h>
#include <driver/sdspi_host.h>
#include <esp_log.h>
#include <esp_vfs_fat.h>
#include <sdmmc_cmd.h>
#include <sys/stat.h>

#include <cstdio>
#include <ctime>
#include <mutex>
#include <string>
#include <vector>

class CartaoWatcher {
public:
    static constexpr const char* kRaiz = "/sdcard";
    static constexpr const char* kPasta = "/sdcard/watcher";

    static CartaoWatcher& Instancia() {
        static CartaoWatcher c;
        return c;
    }

    // O barramento SPI2 já foi iniciado pela placa (compartilhado com o chip de visão)
    bool Montar(spi_host_device_t host, gpio_num_t cs) {
        std::lock_guard<std::mutex> trava(trava_);
        if (cartao_ != nullptr) {
            return true;
        }
        sdmmc_host_t h = SDSPI_HOST_DEFAULT();
        h.slot = host;
        sdspi_device_config_t slot = SDSPI_DEVICE_CONFIG_DEFAULT();
        slot.gpio_cs = cs;
        slot.host_id = host;
        esp_vfs_fat_sdmmc_mount_config_t cfg = {};
        cfg.format_if_mount_failed = false;
        cfg.max_files = 6;
        cfg.allocation_unit_size = 16 * 1024;
        if (esp_vfs_fat_sdspi_mount(kRaiz, &h, &slot, &cfg, &cartao_) != ESP_OK) {
            ESP_LOGW(TAG, "microSD não montou");
            cartao_ = nullptr;
            return false;
        }
        for (const char* sub : {"", "/conversas", "/reunioes", "/memoria", "/enviados"}) {
            mkdir((std::string(kPasta) + sub).c_str(), 0775);
        }
        ESP_LOGI(TAG, "microSD montado");
        return true;
    }

    bool Montado() const { return cartao_ != nullptr; }

    // ------------------------------------------------------------ conversas

    void RegistrarConversa(const std::string& papel, const std::string& texto) {
        if (!Montado() || texto.empty() || papel == "saudacao" || papel == "carregando") {
            return;
        }
        time_t agora = time(nullptr);
        struct tm t;
        localtime_r(&agora, &t);
        char hora[16];
        strftime(hora, sizeof(hora), "%H:%M:%S", &t);
        const char* quem = papel == "user" ? "Você" : (papel == "assistant" ? "Ollie" : "Aviso");
        std::lock_guard<std::mutex> trava(trava_);
        pendente_ += std::string(hora) + " " + quem + ": " + texto + "\n";
    }

    // Grava o que foi acumulado (chamado a cada poucos segundos pelo laço das ações)
    void Descarregar() {
        std::string texto;
        {
            std::lock_guard<std::mutex> trava(trava_);
            texto.swap(pendente_);
        }
        if (texto.empty() || !Montado()) {
            return;
        }
        time_t agora = time(nullptr);
        struct tm t;
        localtime_r(&agora, &t);
        char nome[64];
        strftime(nome, sizeof(nome), "/sdcard/watcher/conversas/%Y-%m-%d.txt", &t);
        if (FILE* f = fopen(nome, "a")) {
            fwrite(texto.data(), 1, texto.size(), f);
            fclose(f);
        }
    }

    // ------------------------------------------------------------ reuniões

    std::string IniciarReuniao() {
        std::lock_guard<std::mutex> trava(trava_);
        if (!Montado()) {
            return "";
        }
        time_t agora = time(nullptr);
        struct tm t;
        localtime_r(&agora, &t);
        char nome[64];
        strftime(nome, sizeof(nome), "/sdcard/watcher/reunioes/%Y%m%d-%H%M%S.wopus", &t);
        reuniao_ = fopen(nome, "wb");
        if (reuniao_ == nullptr) {
            return "";
        }
        setvbuf(reuniao_, nullptr, _IOFBF, 16 * 1024);
        fwrite("WOPUS1\n", 1, 7, reuniao_);
        return nome;
    }

    // Um pacote Opus (16 kHz, 60 ms) = [u16 tamanho][dados]; chamado pela tarefa principal
    void GravarPacote(const std::vector<uint8_t>& pacote) {
        std::lock_guard<std::mutex> trava(trava_);
        if (reuniao_ == nullptr || pacote.empty() || pacote.size() > 0xFFFF) {
            return;
        }
        uint16_t n = pacote.size();
        fwrite(&n, 1, 2, reuniao_);
        fwrite(pacote.data(), 1, n, reuniao_);
    }

    void PararReuniao() {
        std::lock_guard<std::mutex> trava(trava_);
        if (reuniao_ != nullptr) {
            fclose(reuniao_);
            reuniao_ = nullptr;
        }
    }

    // ------------------------------------------------------------ arquivos

    static std::vector<std::string> Listar(const std::string& pasta) {
        std::vector<std::string> nomes;
        if (DIR* d = opendir(pasta.c_str())) {
            while (struct dirent* e = readdir(d)) {
                if (e->d_name[0] != '.') {
                    nomes.push_back(e->d_name);
                }
            }
            closedir(d);
        }
        return nomes;
    }

    static bool Ler(const std::string& caminho, std::string& dados) {
        FILE* f = fopen(caminho.c_str(), "rb");
        if (f == nullptr) {
            return false;
        }
        fseek(f, 0, SEEK_END);
        long n = ftell(f);
        fseek(f, 0, SEEK_SET);
        dados.resize(n > 0 ? n : 0);
        size_t lido = n > 0 ? fread(dados.data(), 1, n, f) : 0;
        fclose(f);
        return (long)lido == n;
    }

    static bool LerParte(const std::string& caminho, long offset, size_t max, std::string& dados) {
        FILE* f = fopen(caminho.c_str(), "rb");
        if (f == nullptr) {
            return false;
        }
        fseek(f, offset, SEEK_SET);
        dados.resize(max);
        size_t lido = fread(dados.data(), 1, max, f);
        dados.resize(lido);
        fclose(f);
        return true;
    }

    static long Tamanho(const std::string& caminho) {
        struct stat s;
        return stat(caminho.c_str(), &s) == 0 ? (long)s.st_size : -1;
    }

    // Move um arquivo para outra pasta de /sdcard/watcher (ex.: "/enviados/")
    static void Mover(const std::string& caminho, const char* destino) {
        auto barra = caminho.find_last_of('/');
        std::string novo = std::string(kPasta) + destino + caminho.substr(barra + 1);
        remove(novo.c_str());
        rename(caminho.c_str(), novo.c_str());
    }

    static bool Escrever(const std::string& caminho, const std::string& dados) {
        FILE* f = fopen(caminho.c_str(), "wb");
        if (f == nullptr) {
            return false;
        }
        size_t escrito = fwrite(dados.data(), 1, dados.size(), f);
        fclose(f);
        return escrito == dados.size();
    }

private:
    static constexpr const char* TAG = "CartaoWatcher";
    sdmmc_card_t* cartao_ = nullptr;
    FILE* reuniao_ = nullptr;
    std::string pendente_;
    std::mutex trava_;
};

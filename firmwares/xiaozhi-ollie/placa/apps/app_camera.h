// App "Câmera": tira uma foto com a câmera do Watcher e mostra o que a IA vê nela. Cada foto fica no microSD,
// organizada por mês (watcher/fotos/AAAA-MM/AAAA-MM-DD_HH-MM-SS.jpg + .txt com a descrição), e no Mac (com a
// descrição; vai para o iCloud pelo Backup). A tela redonda
// não mostra a imagem (sem decodificador de JPEG no firmware); a lista mostra quando e a descrição.
#pragma once

#include <atomic>
#include <vector>

#include <ctime>
#include <sys/stat.h>

#include "../idioma_watcher.h"
#include "../nucleo_apps.h"
#include "../sscma_camera.h"  // a câmera da placa (apps/ fica dentro da pasta da placa)

class AppCamera : public AppWatcher {
public:
    const char* Nome() const override { return TR("Câmera", "Camera", "相机", "Cámara"); }
    const char* Id() const override { return "camera"; }
    const char* Icone() const override { return MATERIAL_SYMBOLS_PHOTO_CAMERA; }
    std::string Detalhe() const override { return TR("Foto e o que a IA vê", "Photo and what AI sees", "拍照并让 AI 描述", "Foto y lo que ve la IA"); }

    void Abrir(ContextoApps& c) override {
        tela_ = Tela::Carregando;
        c.painel.MostrarStatus(Nome(), PainelWatcher::Status::Carregando, TR("Carregando fotos…", "Loading photos…", "正在加载照片…", "Cargando fotos…"));
        pedido_ = Pedido::Lista;
    }

    void Girar(ContextoApps& c, int passo) override {
        if (tela_ != Tela::Carregando) {
            c.painel.Mover(passo);
        }
    }

    bool Clicar(ContextoApps& c) override {
        int i = c.painel.Selecionado();
        if (tela_ == Tela::Carregando) {
            return true;
        }
        if (tela_ == Tela::Lista) {
            if (i == 0) {
                return false;  // Voltar
            }
            if (i == 1) {  // tirar foto
                tela_ = Tela::Carregando;
                c.painel.MostrarStatus(Nome(), PainelWatcher::Status::Carregando,
                                       TR("Tirando a foto e perguntando à IA…", "Taking the photo and asking the AI…",
                                          "正在拍照并询问 AI…", "Haciendo la foto y preguntando a la IA…"));
                pedido_ = Pedido::Foto;
                return true;
            }
            if (i - 2 < (int)fotos_.size()) {
                const auto& f = fotos_[i - 2];
                tela_ = Tela::Texto;
                c.painel.MostrarTexto(Nome(), f.detalhe + "\n\n" + f.texto, {TR("Voltar", "Back", "返回", "Volver")});
            }
            return true;
        }
        MostrarLista(c);  // Texto ou Resultado: volta à lista
        return true;
    }

    bool Voltar(ContextoApps& c) override {
        if (tela_ == Tela::Texto || tela_ == Tela::Resultado) {
            MostrarLista(c);
            return true;
        }
        return false;
    }

    void Tique(ContextoApps& c) override {
        Pedido p = pedido_.exchange(Pedido::Nada);
        if (p == Pedido::Lista) {
            BuscarLista(c);
        } else if (p == Pedido::Foto) {
            TirarFoto(c);
        }
    }

private:
    enum class Tela { Carregando, Lista, Texto, Resultado };
    enum class Pedido { Nada, Lista, Foto };
    struct Foto {
        std::string titulo, detalhe, texto;
    };
    std::atomic<Tela> tela_{Tela::Carregando};
    std::atomic<Pedido> pedido_{Pedido::Nada};
    std::vector<Foto> fotos_;

    void BuscarLista(ContextoApps& c) {
        fotos_.clear();
        std::string corpo;
        if (RedeWatcher::Pedir("GET", "/watcher/fotos", "", corpo)) {
            cJSON* raiz = cJSON_Parse(corpo.c_str());
            cJSON* lista = raiz ? cJSON_GetObjectItem(raiz, "fotos") : nullptr;
            cJSON* f = nullptr;
            cJSON_ArrayForEach(f, lista) {
                fotos_.push_back({RedeWatcher::Campo(f, "titulo"), RedeWatcher::Campo(f, "detalhe"), RedeWatcher::Campo(f, "texto")});
            }
            cJSON_Delete(raiz);
        }
        MostrarLista(c);
    }

    void MostrarLista(ContextoApps& c) {
        tela_ = Tela::Lista;
        std::vector<PainelWatcher::Item> itens = {
            {TR("Voltar", "Back", "返回", "Volver"), "", MATERIAL_SYMBOLS_ARROW_BACK},
            {TR("Tirar foto", "Take photo", "拍照", "Hacer foto"), TR("E ver o que a IA enxerga", "And see what the AI sees", "看看 AI 看到什么", "Y ver lo que ve la IA"),
             MATERIAL_SYMBOLS_PHOTO_CAMERA}};
        for (const auto& f : fotos_) {
            itens.push_back({f.titulo, f.detalhe, MATERIAL_SYMBOLS_IMAGE});
        }
        c.painel.MostrarLista(Nome(), itens, 1);
    }

    // Salva o JPEG em watcher/fotos/AAAA-MM/AAAA-MM-DD_HH-MM-SS.jpg; devolve o caminho (vazio sem cartão)
    static std::string SalvarNoCartao(const std::string& jpeg) {
        if (jpeg.empty() || !CartaoWatcher::Instancia().Montado()) {
            return "";
        }
        time_t agora = time(nullptr);
        struct tm t;
        localtime_r(&agora, &t);
        char mes[16], nome[48];
        strftime(mes, sizeof(mes), "%Y-%m", &t);
        strftime(nome, sizeof(nome), "%Y-%m-%d_%H-%M-%S.jpg", &t);
        std::string pasta = std::string(CartaoWatcher::kPasta) + "/fotos";
        mkdir(pasta.c_str(), 0775);
        pasta += std::string("/") + mes;
        mkdir(pasta.c_str(), 0775);
        std::string caminho = pasta + "/" + nome;
        return CartaoWatcher::Escrever(caminho, jpeg) ? caminho : "";
    }

    void TirarFoto(ContextoApps& c) {
        auto camera = Board::GetInstance().GetCamera();
        std::string descricao;
        bool ok = false;
        std::string arquivo;
        if (camera != nullptr && camera->Capture()) {
            arquivo = SalvarNoCartao(static_cast<SscmaCamera*>(camera)->UltimaFotoJpeg());
            auto r = camera->Explain(TR("Descreva em português do Brasil, em até 3 frases, o que aparece nesta foto.",
                                        "Describe in English, in up to 3 sentences, what is in this photo.",
                                        "用中文、最多 3 句话描述这张照片里的内容。",
                                        "Describe en español, en hasta 3 frases, lo que aparece en esta foto."));
            if (r) {
                cJSON* raiz = cJSON_Parse(r->c_str());
                descricao = RedeWatcher::Campo(raiz, "response");
                ok = !descricao.empty();
                cJSON_Delete(raiz);
            }
        }
        if (!arquivo.empty()) {  // a descrição ao lado da foto
            std::string txt = arquivo.substr(0, arquivo.size() - 4) + ".txt";
            CartaoWatcher::Escrever(txt, ok ? descricao + "\n" : std::string("(sem descrição: a IA não respondeu)\n"));
        }
        DiagnosticoWatcher::Marcar("camera foto %s %s", ok ? "ok" : "falhou", arquivo.empty() ? "sem cartao" : "salva");
        tela_ = Tela::Resultado;
        c.painel.MostrarStatus(Nome(), ok ? PainelWatcher::Status::Sucesso : PainelWatcher::Status::Erro,
                               ok ? descricao.substr(0, 300)
                                  : TR("Não consegui tirar ou analisar a foto agora.", "Couldn't take or analyze the photo now.",
                                       "暂时无法拍照或分析照片。", "No he podido hacer o analizar la foto ahora."),
                               {TR("Voltar", "Back", "返回", "Volver")});
    }
};

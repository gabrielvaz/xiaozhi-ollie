# Apps da gaveta do Watcher

Três cliques na roda abrem a gaveta. Girar escolhe, clicar abre. Cada app é um arquivo em `apps/`, registrado com uma linha em `registro_apps.h`. O `aplicar_patches.py` copia esta pasta para `main/boards/sensecap-watcher/` a cada compilação.

## Arquivos

| Arquivo | Papel |
|---|---|
| `nucleo_apps.h` | Interface `AppWatcher`, `ContextoApps` (tela, voltar, fechar, perguntar ao Ollie, avisar), `RedeWatcher` (pedidos ao Mac com token) e `ConfigWatcher` (ajustes salvos) |
| `gaveta_watcher.h` | Lista os apps, encaminha roda e cliques, roda `Tique`/`Fundo` a cada 500 ms numa tarefa própria |
| `painel_watcher.h` | Telas prontas: `MostrarLista`, `MostrarTexto` (com botões), `MostrarValor`/`AtualizarValor`, `MostrarUso`, `MostrarGravacao`, `Mover`, `Fechar` |
| `cartao_watcher.h` | microSD: registro das conversas, cópia das reuniões, ler/escrever arquivos |
| `registro_apps.h` | **Onde se adiciona um app** (ordem = ordem na gaveta) |
| `apps/` | Os apps |

## Textos e idiomas

Todo texto que aparece na tela ou é falado usa `TR("português", "English", "中文", "español")`, de `idioma_watcher.h` (`#include "../idioma_watcher.h"` dentro de `apps/`). O build escolhe um idioma (`compilar.sh --idioma`) e só aquele texto entra no binário. Não traduza chaves de configuração, JSON, caminhos nem valores comparados com o servidor. Use `Fontes::Grande()` e `Fontes::Pequena()`, que já trocam para a fonte CJK em chinês.

## Ciclo de vida de um app

| Método | Quando | Regra |
|---|---|---|
| `Nome()`, `Detalhe()` | Ao desenhar a gaveta | `Detalhe` pode ser dinâmico ("Correndo: 03:12") |
| `Abrir(c)` | Ao escolher o app | Desenhe a primeira tela |
| `Girar(c, passo)` | Cada passo da roda (+1 horário) | Só mude estado e tela; nada de rede |
| `Clicar(c)` | Cada clique | `return false` volta para a gaveta |
| `Tique(c)` | A cada 500 ms com o app aberto | Pode usar rede e cartão |
| `Fundo(c)` | A cada 500 ms sempre | Para timers e sincronizações; com a gaveta aberta, evite mexer na tela |
| `Visivel()` | — | `false` = serviço sem item na gaveta (ex.: `servico_avisos.h`) |
| `PrendeTela()` | — | `true` = 3 cliques não fecham (ex.: gravação) |

`Girar`/`Clicar` rodam na tarefa dos botões. Para buscar algo no Mac, marque uma flag (`std::atomic<bool>`) e faça o pedido no `Tique`, como em `app_uso_claude.h`.

## Exemplo mínimo

```cpp
// apps/app_contador.h
#pragma once
#include "../nucleo_apps.h"

class AppContador : public AppWatcher {
public:
    const char* Nome() const override { return "Contador"; }
    void Abrir(ContextoApps& c) override { c.painel.MostrarValor("Contador", std::to_string(n_), "gire para contar", {"Zerar", "Voltar"}); }
    void Girar(ContextoApps& c, int passo) override { n_ += passo; c.painel.AtualizarValor(std::to_string(n_)); }
    bool Clicar(ContextoApps& c) override {
        if (c.painel.Selecionado() == 0) { n_ = 0; Abrir(c); return true; }
        return false;
    }
private:
    int n_ = 0;
};
```

E em `registro_apps.h`: `#include "apps/app_contador.h"` e `gaveta.Registrar(std::make_unique<AppContador>());`.

## Pedidos ao Mac

`RedeWatcher::Pedir("GET", "/watcher/<rota>", "", corpo)` usa o mesmo token do canal de voz. As rotas ficam no servidor, em `servidor/extras/core/api/`, registradas no patch de rotas de `servidor/patches/traduzir_servidor.py`. Hoje existem: `avisos`, `sessoes`, `uso`, `memoria`, `memoria/audio/{nome}`, `upload`.

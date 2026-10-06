"""Mede o Clawd em cada cena do simulador e monta a folha de conferência.

- O simulador roda duas vezes: em saida/medida/ toda pose vira o Clawd parado e sem acessórios
  (OLLIE_MEDIR=1), e é ali que o corpo é medido (laranja D97757); em saida/ ficam as cenas com as
  poses de verdade, onde os retângulos são desenhados. Texto: a caixa reservada (bottom_bar_), que o
  simulador grava em saida/medida/<cena>.txt.
- Regra (placa/layout_mascote.h): corpo do Clawd no centro da tela em todos os fluxos, e a caixa do
  texto inteira dentro do círculo. Tolerância: 2 px (RGB565 e arredondamento).
Gera saida/*.png (com o círculo da tela e as guias do centro), saida/folha.png com os fluxos e
saida/poses.png com todas as poses (para conferir GIFs novos a olho).
Sai com erro se alguma cena fugir da regra.
"""
import collections
import glob
import os
import sys

from PIL import Image, ImageDraw

LADO = 412
C = LADO / 2
TOL = 2


def laranja(p):
    return abs(p[0] - 217) < 16 and abs(p[1] - 119) < 16 and abs(p[2] - 87) < 16


def corpo(img):
    """Retângulo do corpo do Clawd parado (staticstate): a largura mais comum entre as linhas com laranja,
    só acima de y=300 (embaixo ficam botões laranja), no maior bloco contínuo (o título dos apps é laranja)."""
    px = img.load()
    faixas = {}
    for y in range(300):
        xs = [x for x in range(LADO) if laranja(px[x, y])]
        if len(xs) >= 10:
            faixas[y] = (xs[0], xs[-1])
    if not faixas:
        return None
    largura = collections.Counter(f[1] - f[0] for f in faixas.values()).most_common(1)[0][0]
    linhas = sorted(y for y, f in faixas.items() if abs((f[1] - f[0]) - largura) <= 1)
    blocos, atual = [], [linhas[0]]
    for y in linhas[1:]:
        if y - atual[-1] <= 6:
            atual.append(y)
        else:
            blocos.append(atual)
            atual = [y]
    blocos.append(atual)
    bloco = max(blocos, key=len)
    x0 = min(faixas[y][0] for y in bloco)
    x1 = max(faixas[y][1] for y in bloco)
    base = max(y for y in range(bloco[0], 300) if any(laranja(px[x, y]) for x in range(x0, x1 + 1)))
    return x0, x1, bloco[0], base


def folha(imagens, caminho, col):
    if not imagens:
        return
    w, h = imagens[0].size
    lin = (len(imagens) + col - 1) // col
    f = Image.new("RGB", (col * w, lin * h), (20, 20, 20))
    for i, m in enumerate(imagens):
        f.paste(m, ((i % col) * w, (i // col) * h))
    f.save(caminho)


def main():
    falhas = []
    fluxos, poses = [], []
    for c in sorted(glob.glob("saida/*.ppm")):
        nome = os.path.basename(c)[:-4]
        img = Image.open(c).convert("RGB")
        medida = f"saida/medida/{nome}.ppm"
        # mede na rodada sem acessórios; poses (só para ver) e sucesso/erro dos apps não têm o que medir
        m = None
        if os.path.exists(medida) and not nome.startswith("p-") and not nome.endswith("-sucesso"):
            m = corpo(Image.open(medida).convert("RGB"))
        d = ImageDraw.Draw(img)
        linha = f"{nome:30s} "
        if m:
            x0, x1, y0, y1 = m
            cx = (x0 + x1 + 1) / 2
            caixa = open(medida[:-4] + ".txt").read().split()
            t = tuple(int(v) for v in caixa) if caixa else None  # x1 x2 y1 y2
            # centro pelo topo do corpo e pela largura (as perninhas podem estar cobertas: notebook, livro)
            altura = round((x1 - x0 + 1) * 66 / 84)  # corpo 84x66 no GIF (gerar.py)
            bloco = (y0, y0 + altura)
            cy = sum(bloco) / 2
            if t:  # cantos de baixo da caixa do texto dentro do círculo
                meia = ((C ** 2 - (t[3] - C) ** 2) ** 0.5) if abs(t[3] - C) < C else 0
                cabe = max(C - t[0], t[1] - C) <= meia
                if not cabe:
                    falhas.append(nome + " (texto sai do círculo)")
            dx, dy = cx - C, cy - C
            ok_x = abs(dx) <= TOL
            if nome.endswith("-com-botoes"):  # painel com botões: ícone 40 px acima do centro
                dy += 40
            ok_y = abs(dy) <= TOL or nome.startswith("00-")  # abertura: Clawd em cima do logo
            if not (ok_x and ok_y):
                falhas.append(nome)
            d.rectangle([x0, y0, x1, y1], outline=(0, 255, 0) if ok_x and ok_y else (255, 0, 0))
            if t:
                d.rectangle([t[0], t[2], t[1], t[3]], outline=(0, 160, 0))
            linha += (f"corpo x {x0}-{x1} (desvio {dx:+.1f})  corpo y {bloco[0]}-{bloco[1]} "
                      f"(desvio {dy:+.1f})  texto {f'y {t[2]}-{t[3]}' if t else '-'}  {'ok' if ok_x and ok_y else 'FORA DO CENTRO'}")
        elif not nome.startswith("p-"):
            linha += "sem Clawd"
        if not nome.startswith("p-"):
            print(linha)
        d.ellipse([0, 0, LADO - 1, LADO - 1], outline=(60, 160, 255))
        d.line([C, 0, C, LADO], fill=(0, 90, 160))
        d.line([0, C, LADO, C], fill=(0, 90, 160))
        mascara = Image.new("L", (LADO, LADO), 0)
        ImageDraw.Draw(mascara).ellipse([0, 0, LADO - 1, LADO - 1], fill=255)
        fundo = Image.new("RGB", (LADO, LADO), (40, 40, 40))
        fundo.paste(img, (0, 0), mascara)
        ImageDraw.Draw(fundo).text((8, 4), nome, fill=(255, 255, 0))
        fundo.save(f"saida/{nome}.png")
        os.remove(c)
        os.remove(c[:-4] + ".txt")
        (poses if nome.startswith("p-") else fluxos).append(fundo)
    folha(fluxos, "saida/folha.png", 4)
    folha([p.resize((LADO // 2, LADO // 2)) for p in poses], "saida/poses.png", 8)
    print(f"\n{len(falhas)} cena(s) fora do centro" + (": " + ", ".join(falhas) if falhas else ""))
    sys.exit(1 if falhas else 0)


main()

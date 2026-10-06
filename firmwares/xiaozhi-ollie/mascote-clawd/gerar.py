"""Gera o Clawd (mascote do Claude Code) em pixel art: as 21 emoções do XiaoZhi,
os estados do sistema e poses extras (trabalhando, correndo, de óculos...).

Saída: um GIF animado 128x128 por emoção em ./emocoes/ (nomes exigidos pelo firmware)
e uma prévia ampliada em ./previa.png.
Uso: uv run --with pillow python gerar.py
"""

from pathlib import Path

from PIL import Image, ImageDraw

AQUI = Path(__file__).resolve().parent
SAIDA = AQUI / "emocoes"
LADO = 128
PX = 6                      # tamanho de cada "pixel" do desenho
GRADE_W, GRADE_H = 20, 18   # grade do desenho em pixels lógicos

LARANJA = (217, 119, 87)    # laranja-argila do Claude
LARANJA_ESCURO = (178, 88, 62)
OLHO = (30, 24, 22)
BRANCO = (255, 255, 255)
ROSA = (240, 140, 150)
VERMELHO = (220, 50, 60)
AZUL = (90, 170, 240)
AMARELO = (255, 205, 70)
CINZA = (200, 200, 200)
AZUL_ESCURO = (50, 110, 190)

EMOCOES = [
    "neutral", "happy", "laughing", "funny", "sad", "angry", "crying", "loving",
    "embarrassed", "surprised", "shocked", "thinking", "winking", "cool", "relaxed",
    "delicious", "kissy", "confident", "sleepy", "silly", "confused",
    # estados do sistema: iniciando/conectando, alerta e parado
    "robot_2", "warning", "staticstate",
    # estados da conversa (o firmware troca sozinho): conectando, ouvindo e falando
    "conectando", "ouvindo", "falando",
    # carregador conectado: o Clawd leva um choque
    "choque",
    # trabalhando (sessões do Claude Code rodando): no terminal e no teclado
    "codando", "teclando",
    # poses extras (o servidor escolhe pelo emoji da resposta ou pelo que está acontecendo)
    "waving", "nerd", "reading", "working", "running", "coffee", "party", "music",
    "searching", "idea", "rocket", "bug", "sunny", "rainy", "recording", "celebrating",
    # andando de skate (indo buscar algo: atualização, backup), chateado (algo falhou) e
    # offline (sem Wi-Fi ou sem servidor)
    "skate", "chateado", "offline",
    # roda girando (volume): aumentando e diminuindo
    "volume_mais", "volume_menos",
    # sem Wi-Fi configurado (modo de configuração de rede): confuso, com "?" e o Wi-Fi falhando
    "sem_wifi",
]

CINZA_ESCURO = (70, 70, 80)
VERDE = (90, 200, 110)
MARROM = (150, 95, 60)


class Tela:
    def __init__(self, dy=0):
        self.img = Image.new("RGBA", (LADO, LADO), (0, 0, 0, 0))
        self.d = ImageDraw.Draw(self.img)
        self.ox = (LADO - GRADE_W * PX) // 2
        self.oy = (LADO - GRADE_H * PX) // 2 + dy

    def px(self, x, y, cor, w=1, h=1):
        x0, y0 = self.ox + x * PX, self.oy + y * PX
        self.d.rectangle([x0, y0, x0 + w * PX - 1, y0 + h * PX - 1], fill=cor)


def corpo(t, cor=LARANJA, bracos_y=8, pernas=True):
    t.px(3, 4, cor, 14, 9)                 # corpo
    if bracos_y is not None:               # None: a pose desenha os braços
        t.px(1, bracos_y, cor, 2, 2)       # braço esquerdo
        t.px(17, bracos_y, cor, 2, 2)      # braço direito
    if pernas:
        for x in (4, 6, 13, 15):           # quatro perninhas
            t.px(x, 13, cor, 1, 2)


def olhos(t, tipo="normal", dx=0, dy=0):
    e, d = 6 + dx, 13 + dx
    y = 6 + dy
    if tipo == "normal":
        t.px(e, y, OLHO, 1, 2); t.px(d, y, OLHO, 1, 2)
    elif tipo == "fechado":
        t.px(e - 1, y + 1, OLHO, 2, 1); t.px(d, y + 1, OLHO, 2, 1)
    elif tipo == "feliz":          # ^ ^
        for x in (e, d):
            t.px(x - 1, y + 1, OLHO); t.px(x, y, OLHO); t.px(x + 1, y + 1, OLHO)
    elif tipo == "triste":         # olhos baixos com sobrancelha caída
        t.px(e, y + 1, OLHO, 1, 2); t.px(d, y + 1, OLHO, 1, 2)
        t.px(e - 1, y - 1, OLHO); t.px(e, y - 2 + 1, OLHO)
        t.px(d + 1, y - 1, OLHO); t.px(d, y - 1, OLHO)
    elif tipo == "bravo":
        t.px(e, y + 1, OLHO, 1, 2); t.px(d, y + 1, OLHO, 1, 2)
        t.px(e - 1, y - 1, OLHO); t.px(e, y, OLHO)
        t.px(d + 1, y - 1, OLHO); t.px(d, y, OLHO)
    elif tipo == "grande":
        t.px(e - 1, y - 1, OLHO, 2, 3); t.px(d, y - 1, OLHO, 2, 3)
        t.px(e - 1, y - 1, BRANCO); t.px(d, y - 1, BRANCO)
    elif tipo == "coracao":
        for x in (e - 1, d):
            t.px(x, y, VERMELHO); t.px(x + 2, y, VERMELHO)
            t.px(x, y + 1, VERMELHO, 3, 1); t.px(x + 1, y + 2, VERMELHO)
    elif tipo == "piscando":
        t.px(e, y, OLHO, 1, 2); t.px(d, y + 1, OLHO, 2, 1)
    elif tipo == "oculos":
        t.px(4, y, OLHO, 12, 1); t.px(4, y + 1, OLHO, 5, 2); t.px(11, y + 1, OLHO, 5, 2)
        t.px(5, y + 1, (90, 90, 110))
        t.px(12, y + 1, (90, 90, 110))
    elif tipo == "meio":           # pálpebras a meia altura (confiante / sonolento)
        t.px(e - 1, y + 1, OLHO, 2, 1); t.px(d, y + 1, OLHO, 2, 1)
        t.px(e - 1, y, LARANJA_ESCURO, 2, 1); t.px(d, y, LARANJA_ESCURO, 2, 1)
    elif tipo == "doido":
        t.px(e - 1, y - 1, OLHO, 2, 3); t.px(d, y + 1, OLHO, 1, 1)
    elif tipo == "confuso":
        t.px(e, y, OLHO, 1, 2); t.px(d, y + 1, OLHO, 2, 1)
        t.px(d, y - 1, OLHO, 2, 1)


def boca(t, tipo):
    if tipo == "sorriso":
        t.px(8, 10, OLHO); t.px(9, 11, OLHO, 2, 1); t.px(11, 10, OLHO)
    elif tipo == "aberta":
        t.px(8, 10, OLHO, 4, 2); t.px(9, 11, VERMELHO, 2, 1)
    elif tipo == "triste":
        t.px(8, 11, OLHO); t.px(9, 10, OLHO, 2, 1); t.px(11, 11, OLHO)
    elif tipo == "o":
        t.px(9, 10, OLHO, 2, 2)
    elif tipo == "lingua":
        t.px(8, 10, OLHO, 4, 1); t.px(10, 11, ROSA, 2, 2)
    elif tipo == "beijo":
        t.px(10, 10, OLHO); t.px(11, 11, OLHO); t.px(10, 12, OLHO)
    elif tipo == "reta":
        t.px(8, 11, OLHO, 4, 1)
    elif tipo == "sorriso_lado":
        t.px(9, 11, OLHO, 2, 1); t.px(11, 10, OLHO)


def bochechas(t):
    t.px(4, 9, ROSA, 2, 1); t.px(14, 9, ROSA, 2, 1)


def texto_pixel(t, x, y, padrao, cor):
    for j, linha in enumerate(padrao):
        for i, c in enumerate(linha):
            if c == "#":
                t.px(x + i, y + j, cor)


INTERROGACAO = ["##.", "..#", ".#.", "...", ".#."]
EXCLAMACAO = ["#", "#", "#", ".", "#"]
Z = ["###", "..#", ".#.", "#..", "###"]
CORACAO = [".#.#.", "#####", ".###.", "..#.."]


def pose(nome, f):
    """Poses extras: o Clawd fazendo alguma coisa. Quadro f (0..3)."""
    par = f % 2
    if nome == "waving":                  # acenando (boas-vindas)
        t = Tela()
        corpo(t, bracos_y=None)
        t.px(1, 8, LARANJA, 2, 2)
        t.px(17, 5 if par else 6, LARANJA, 2, 2); t.px(19 if par else 18, 4 if par else 5, LARANJA)
        olhos(t, "feliz"); boca(t, "sorriso"); bochechas(t)
        return t.img
    if nome in ("nerd", "reading"):       # óculos redondos (lendo um livro)
        t = Tela()
        corpo(t, bracos_y=None if nome == "reading" else 8)
        for x in (5, 12):                 # armação ao redor de cada olho
            t.px(x, 5, OLHO, 3, 1); t.px(x, 8, OLHO, 3, 1)
            t.px(x, 6, OLHO, 1, 2); t.px(x + 2, 6, OLHO, 1, 2)
        t.px(8, 6, OLHO, 4, 1); t.px(3, 6, OLHO, 2, 1); t.px(15, 6, OLHO, 2, 1)
        dy = 1 if nome == "reading" else 0
        t.px(6, 6 + dy, OLHO); t.px(13, 6 + dy, OLHO)
        if f == 2:
            t.px(5, 5, BRANCO); t.px(12, 5, BRANCO)
        if nome == "reading":
            t.px(2, 11, MARROM, 16, 4)                              # capa
            t.px(3, 11, BRANCO, 6, 3); t.px(11, 11, BRANCO, 6, 3)   # páginas
            t.px(9, 11, CINZA, 2, 3)                                # lombada
            for y in (12, 13):                                      # linhas de texto
                t.px(4, y, CINZA, 4 - par * (y - 12), 1); t.px(12, y, CINZA, 4 - (1 - par) * (y - 12), 1)
            t.px(1, 11, LARANJA, 2, 2); t.px(17, 11, LARANJA, 2, 2)
        else:
            boca(t, "sorriso")
        return t.img
    if nome == "working":                 # notebook, digitando
        t = Tela()
        corpo(t, bracos_y=11)
        t.px(1, 11 - par, LARANJA, 2, 2); t.px(17, 10 + par, LARANJA, 2, 2)
        olhos(t, "normal", dy=0)
        t.px(4, 9, CINZA, 12, 4); t.px(3, 13, CINZA_ESCURO, 14, 1)   # tampa e base
        t.px(9, 10, LARANJA, 2, 2)                                   # logo
        t.px(6, 6, AZUL); t.px(13, 6, AZUL)                          # brilho da tela nos olhos
        texto_pixel(t, 15, -1, ["#..", ".#.", "#.."], VERDE)   # prompt ">_" piscando
        if par:
            t.px(18, 1, VERDE, 2, 1)
        return t.img
    if nome == "running":                 # correndo
        t = Tela(dy=-PX // 2 if par else 0)
        t.px(3, 4, LARANJA, 14, 9)
        t.px(1, 6 if par else 9, LARANJA, 2, 2); t.px(17, 9 if par else 6, LARANJA, 2, 2)
        pernas = [(3, 13), (6, 14), (13, 13), (16, 14)] if par else [(4, 14), (7, 13), (12, 14), (15, 13)]
        for x, y in pernas:
            t.px(x, y, LARANJA, 1 if y == 14 else 2, 1 if y == 14 else 2)
        olhos(t, "normal", dx=1); boca(t, "sorriso_lado")
        for y, w in ((5, 2), (8, 3), (11, 2)):          # linhas de velocidade
            t.px(-1 - (f % 2), y, CINZA, w, 1)
        t.px(16, 2 + f % 3, AZUL)                        # suor
        return t.img
    if nome == "coffee":                  # cafezinho
        t = Tela()
        corpo(t)
        olhos(t, "fechado" if f != 0 else "meio"); boca(t, "sorriso"); bochechas(t)
        t.px(16, 9, BRANCO, 3, 3); t.px(19, 10, BRANCO); t.px(17, 10, MARROM)
        for i, dx in enumerate((0, 1, 0)):
            t.px(16 + ((dx + f) % 2) + i, 8 - i - (f % 2), CINZA)
        return t.img
    if nome in ("party", "celebrating"):  # festa: chapéu e confete / braços para cima
        t = Tela(dy=-PX // 2 if par else 0)
        corpo(t, bracos_y=None)
        topo = 0 if par else 1                       # braços erguidos, balançando
        t.px(1, topo, LARANJA, 2, 5 - topo); t.px(17, topo, LARANJA, 2, 5 - topo)
        olhos(t, "feliz"); boca(t, "aberta"); bochechas(t)
        if nome == "party":
            t.px(9, 1, AZUL, 2, 1); t.px(8, 2, AMARELO, 4, 1); t.px(8, 3, AZUL, 4, 1); t.px(9, 0, VERMELHO, 2, 1)
        cores = (VERMELHO, AMARELO, AZUL, VERDE, ROSA)
        pontos = [(0, 0), (4, 1), (15, 0), (19, 2), (2, 15), (18, 14), (12, 16), (6, 16)]
        for i, (x, y) in enumerate(pontos):
            t.px(x, (y + f) % 17, cores[(i + f) % len(cores)])
        return t.img
    if nome == "music":                   # fones, curtindo
        t = Tela(dy=PX // 2 if par else 0)
        corpo(t)
        t.px(4, 2, CINZA_ESCURO, 12, 1); t.px(3, 3, CINZA_ESCURO, 1, 2); t.px(16, 3, CINZA_ESCURO, 1, 2)
        t.px(2, 5, CINZA_ESCURO, 2, 4); t.px(16, 5, CINZA_ESCURO, 2, 4)
        t.px(2, 6, VERMELHO, 1, 2); t.px(17, 6, VERMELHO, 1, 2)
        olhos(t, "fechado"); boca(t, "sorriso")
        nota = [".##", ".#.", "##."]
        texto_pixel(t, 0 if par else 17, 0 + f % 2, nota, AMARELO)
        return t.img
    if nome == "searching":               # olhando pela lupa, o olho aparece gigante no vidro
        t = Tela(dy=(0, 0, -1, 0, 0, 1)[f] * PX // 3)
        corpo(t, bracos_y=None)
        t.px(1, 8, LARANJA, 2, 2)
        dx, dy = (0, -1, -2, -3, -2, -1)[f], (0, 0, 1, 1, 0, 0)[f]
        x, y = 12 + dx, 2 + dy                                     # canto da lupa
        t.px(6 + (0, 0, -1, -1, 0, 1)[f], 6, OLHO, 1, 2)            # o outro olho acompanha
        boca(t, "o" if f in (2, 3) else "reta")
        t.px(x + 1, y, CINZA_ESCURO, 4, 1); t.px(x + 1, y + 5, CINZA_ESCURO, 4, 1)   # aro
        t.px(x, y + 1, CINZA_ESCURO, 1, 4); t.px(x + 5, y + 1, CINZA_ESCURO, 1, 4)
        t.px(x + 1, y + 1, (175, 215, 245), 4, 4)                  # vidro
        ox = (1, 0, 0, 1, 2, 1)[f]                                 # olho ampliado olhando em volta
        t.px(x + 1 + ox, y + 2, OLHO, 2, 2); t.px(x + 1 + ox, y + 2, BRANCO)
        t.px(x + 4, y + 1, BRANCO)                                 # reflexo
        t.px(x + 6, y + 6, MARROM); t.px(x + 7, y + 7, MARROM)     # cabo
        t.px(x + 7, y + 8, LARANJA, 2, 2)                          # mão segurando
        if f == 3:                                                 # achou alguma coisa
            t.px(2, 1, AMARELO); t.px(1, 2, AMARELO); t.px(3, 2, AMARELO); t.px(2, 3, AMARELO)
        return t.img
    if nome == "skate":                   # andando de skate, o chão passando
        t = Tela(dy=(0, -1, 0, 1, 0, -1)[f] * PX // 3)
        corpo(t, bracos_y=None)
        sobe = f % 2                                               # braços em gangorra, equilibrando
        t.px(1, 6 + sobe, LARANJA, 2, 2); t.px(17, 8 - sobe, LARANJA, 2, 2)
        olhos(t, "feliz" if f in (2, 3) else "normal", dx=1); boca(t, "sorriso")
        t.px(2, 15, MARROM, 16, 1); t.px(1, 14, MARROM); t.px(18, 14, MARROM)   # shape com bicos
        for x in (4, 14):                                          # rodinhas girando
            t.px(x, 16, CINZA, 2, 1); t.px(x + (f % 2), 16, BRANCO)
        for k in range(4):                                         # chão passando para trás
            t.px((k * 6 - f * 2) % 24 - 2, 18, CINZA_ESCURO, 2, 1)
        for y, w in ((5, 2), (9, 3), (12, 2)):                     # vento
            t.px(-1 - (f + y) % 2, y, CINZA, w, 1)
        return t.img
    if nome == "chateado":                # braços cruzados, bufando, pezinho batendo
        t = Tela()
        corpo(t, (226, 108, 82), bracos_y=None, pernas=False)
        for x in (4, 6, 13):
            t.px(x, 13, (226, 108, 82), 1, 2)
        t.px(15, 13 if f % 2 else 14, (226, 108, 82), 1, 2 if f % 2 else 1)   # pé batendo
        olhos(t, "bravo" if f != 2 else "fechado")
        t.px(8, 10, OLHO); t.px(9, 9, OLHO, 2, 1); t.px(11, 10, OLHO)          # boca emburrada
        t.px(1, 10, (226, 108, 82), 2, 2); t.px(17, 11, (226, 108, 82), 2, 2)  # cotovelos
        t.px(3, 11, LARANJA_ESCURO, 10, 1); t.px(7, 12, LARANJA_ESCURO, 10, 1)  # braços cruzados
        rabisco = ([(14, 1), (15, 0), (16, 1), (17, 0), (18, 1)], [(14, 0), (15, 1), (16, 0), (17, 1), (18, 0)])
        for x, y in rabisco[f % 2]:                                # nuvenzinha de raiva
            t.px(x, y, CINZA_ESCURO)
        if f in (1, 3):                                            # bufada saindo dos lados
            t.px(0, 4 - (f == 3), CINZA, 2, 1); t.px(18, 4 - (f == 3), CINZA, 2, 1)
        return t.img
    if nome == "offline":                 # sem conexão: tenta encaixar o plugue e não alcança a tomada
        t = Tela()
        corpo(t, bracos_y=None)
        px_ = (5, 6, 7, 8, 7, 6)[f]                                # plugue indo e voltando
        t.px(1, 7 - (f in (2, 3, 4)), LARANJA, 2, 2); t.px(17, 8, LARANJA, 2, 2)
        olhos(t, "normal", dx=(-1, -1, 0, 0, 0, -1)[f], dy=-1); boca(t, "o" if f == 3 else "reta")
        t.px(-1, 1, CINZA_ESCURO, px_ + 1, 1)                      # cabo da esquerda
        t.px(px_, 0, CINZA, 2, 3)                                  # plugue
        t.px(px_ + 2, 0, AMARELO); t.px(px_ + 2, 2, AMARELO)       # pinos
        t.px(12, 0, CINZA, 3, 3); t.px(12, 0, OLHO); t.px(12, 2, OLHO)   # tomada (furos)
        t.px(15, 1, CINZA_ESCURO, 6, 1)                            # cabo da direita
        if f == 3:                                                 # quase: faísca no vão
            t.px(11, -1, AMARELO); t.px(11, 3, VERMELHO)
        if f in (4, 5):
            t.px(17, 3 + f - 4, AZUL)                              # gotinha de suor
        return t.img
    if nome in ("volume_mais", "volume_menos"):
        # Medidor de volume em cima da cabeça: 4 barras (alturas 1 a 4) que acendem uma a uma ao aumentar
        # e apagam ao diminuir. Aumentando: Clawd animado, braços para cima, pulinho. Diminuindo: tapa
        # os ouvidos e o som vai embora pelos lados
        mais = nome == "volume_mais"
        acesas = (1, 2, 3, 4, 4, 4)[f] if mais else (4, 3, 2, 1, 0, 0)[f]
        t = Tela(dy=-(PX // 2) if mais and par else 0)
        corpo(t, bracos_y=None)
        for i, x in enumerate((3, 7, 11, 15)):
            cor = (VERDE, VERDE, AMARELO, VERMELHO)[i] if i < acesas else CINZA_ESCURO
            t.px(x, 2 - i, cor, 2, i + 1)
        if mais:
            y = 5 if par else 7                                    # braços batendo no ar
            t.px(1, y, LARANJA, 2, 2); t.px(17, y, LARANJA, 2, 2)
            olhos(t, "feliz"); boca(t, "aberta")
        else:
            t.px(1, 5, LARANJA, 2, 3); t.px(17, 5, LARANJA, 2, 3)  # mãos tapando os ouvidos
            t.px(3, 5, LARANJA_ESCURO, 1, 3); t.px(16, 5, LARANJA_ESCURO, 1, 3)
            olhos(t, "fechado" if f in (1, 2, 3) else "meio"); boca(t, "reta")
            if f in (1, 2, 3):                                     # o som indo embora pelos lados
                t.px(0, 8 - f % 2, CINZA); t.px(19, 8 - f % 2, CINZA)
        return t.img
    if nome == "sem_wifi":
        # O símbolo do Wi-Fi tenta acender (ponto, arco do meio, arco de fora), falha com um risco vermelho
        # e apaga; o "?" pula e o Clawd coça a cabeça
        t = Tela()
        corpo(t, bracos_y=None)
        t.px(1, 8, LARANJA, 2, 2)                                  # braço esquerdo parado
        if f % 2:
            t.px(17, 4, LARANJA, 2, 2); t.px(16, 3, LARANJA, 1, 1)  # coçando a cabeça
        else:
            t.px(17, 8, LARANJA, 2, 2)
        olhos(t, "confuso"); boca(t, "reta")
        arcos = [  # (desenho, linha de cima, quadros em que acende); o ponto encosta na cabeça
            (["...#..."], 3, (0, 1, 2, 3, 4)),
            (["..###..", ".#...#."], 1, (1, 2, 3, 4)),
            ([".#####.", "#.....#"], -1, (2, 3, 4)),
        ]
        for padrao, y, acesos in arcos:
            texto_pixel(t, 2, y, padrao, BRANCO if f in acesos else CINZA_ESCURO)
        if f in (3, 4):                                            # falhou: risco vermelho na diagonal
            for i in range(5):
                t.px(1 + i + (i > 1), -1 + i, VERMELHO, 2, 1)
        texto_pixel(t, 13 + (f % 2), -1, INTERROGACAO, AMARELO)    # "?" balançando
        return t.img
    if nome == "idea":                    # lâmpada acendendo
        t = Tela()
        corpo(t)
        olhos(t, "grande"); boca(t, "sorriso")
        luz = AMARELO if f else CINZA
        t.px(9, -1, luz, 2, 1); t.px(8, 0, luz, 4, 2); t.px(9, 2, CINZA_ESCURO, 2, 1)
        if f in (1, 3):
            t.px(6, 0, AMARELO); t.px(13, 0, AMARELO); t.px(7, -1, AMARELO); t.px(12, -1, AMARELO)
        return t.img
    if nome == "rocket":                  # decolando (deploy)
        t = Tela(dy=-(f % 4) * PX // 2)
        corpo(t, pernas=False)
        olhos(t, "grande"); boca(t, "aberta")
        for x in (4, 6, 13, 15):
            t.px(x, 13, AMARELO if (x + f) % 2 else VERMELHO, 1, 2 + (x + f) % 2)
            t.px(x, 15 + (x + f) % 2, VERMELHO)
        return t.img
    if nome == "bug":                     # achou um bug
        t = Tela()
        corpo(t)
        olhos(t, "normal", dx=-1, dy=1); boca(t, "o")
        bx = 15 - f * 3
        t.px(bx, 16, VERDE, 3, 1); t.px(bx + 1, 15, VERDE)
        t.px(bx - 1, 15 + par, OLHO); t.px(bx + 3, 16 - par, OLHO)
        texto_pixel(t, 18, 0, EXCLAMACAO, AMARELO)
        return t.img
    if nome == "sunny":                   # dia de sol
        t = Tela()
        corpo(t)
        olhos(t, "oculos"); boca(t, "sorriso")
        t.px(0, 0, AMARELO, 3, 3)
        raios = [(3, 0), (0, 3), (3, 3)] if par else [(4, 1), (1, 4), (2, -1)]
        for x, y in raios:
            t.px(x, y, AMARELO)
        return t.img
    if nome == "rainy":                   # chuva, guarda-chuva
        t = Tela()
        corpo(t)
        olhos(t, "normal"); boca(t, "reta")
        t.px(4, 1, AZUL, 12, 1); t.px(2, 2, AZUL, 16, 1); t.px(1, 3, AZUL_ESCURO, 18, 1)
        t.px(9, 0, AZUL, 2, 1)
        for x in (0, 19):
            t.px(x, 6 + (f * 2 + x) % 8, AZUL, 1, 2)
        t.px(2, 13 - f % 2 * 4, AZUL)
        return t.img
    if nome == "recording":               # gravando reunião
        t = Tela()
        corpo(t)
        olhos(t, "normal", dx=-1); boca(t, "reta")
        if f in (0, 1):
            t.px(17, 0, VERMELHO, 2, 2)
        t.px(16, 5, CINZA, 2, 3); t.px(16, 6, CINZA_ESCURO, 2, 1); t.px(16, 8, OLHO, 2, 1)
        t.px(16, 9, OLHO, 1, 2)
        return t.img
    raise ValueError(nome)


POSES = set(EMOCOES[EMOCOES.index("waving"):])
ESTADOS = {"conectando": 180, "ouvindo": 220, "falando": 140, "choque": 90, "codando": 260, "teclando": 120,
           "skate": 140, "chateado": 220, "offline": 260, "sad": 320, "searching": 240,
           "volume_mais": 130, "volume_menos": 170, "sem_wifi": 330}   # ms por quadro (6 quadros)


def quadro(emocao, f):
    """Desenha o quadro f (0..3) da emoção."""
    if emocao in POSES:
        return pose(emocao, f)
    sobe = -1 if f % 2 else 0
    t = Tela(dy=sobe * PX // 2 if emocao in ("happy", "laughing", "funny", "loving", "silly") else 0)
    cor = (226, 100, 80) if emocao == "angry" else LARANJA
    corpo(t, cor, bracos_y=7 if (emocao in ("happy", "laughing", "loving") and f % 2) else 8)

    pisca = f == 3
    if emocao == "neutral":
        olhos(t, "fechado" if pisca else "normal")
    elif emocao == "happy":
        olhos(t, "feliz"); boca(t, "sorriso")
    elif emocao == "laughing":
        olhos(t, "feliz"); boca(t, "aberta"); bochechas(t)
    elif emocao == "funny":
        olhos(t, "feliz"); boca(t, "lingua")
    elif emocao == "sad":                # cabisbaixo, nuvenzinha chovendo em cima e uma lágrima
        t = Tela(dy=(0, 0, 1, 1, 1, 0)[f] * PX // 3)    # suspiro lento
        corpo(t, bracos_y=10)                            # braços caídos
        olhos(t, "triste", dy=1 if f in (2, 3, 4) else 0); boca(t, "triste")
        t.px(7, 0, CINZA_ESCURO, 6, 1); t.px(6, 1, CINZA_ESCURO, 8, 1); t.px(8, -1, CINZA_ESCURO, 3, 1)
        for x in (7, 10, 12):                            # garoa caindo da nuvem
            t.px(x, 2 + (f + x) % 2, AZUL)
        if f >= 3:                                       # lágrima escorrendo
            t.px(6, 8 + (f - 3) * 1 + (1 if f in (2, 3, 4) else 0), AZUL, 1, 2)
        return t.img
    elif emocao == "angry":
        olhos(t, "bravo"); boca(t, "reta")
        if f % 2:
            t.px(16, 1, VERMELHO, 1, 2); t.px(18, 2, VERMELHO, 1, 1)
    elif emocao == "crying":
        olhos(t, "triste"); boca(t, "triste")
        t.px(6, 9 + f % 3, AZUL, 1, 2); t.px(13, 9 + (f + 1) % 3, AZUL, 1, 2)
    elif emocao == "loving":
        olhos(t, "coracao"); boca(t, "sorriso")
        texto_pixel(t, 15 + f % 2, 0 - f % 2 + 1, CORACAO, VERMELHO)
    elif emocao == "embarrassed":
        olhos(t, "normal", dx=1 if f % 2 else 0); bochechas(t); boca(t, "reta")
        t.px(16, 3, AZUL, 1, 2)
    elif emocao == "surprised":
        olhos(t, "grande"); boca(t, "o")
    elif emocao == "shocked":
        olhos(t, "grande"); boca(t, "aberta")
        if f % 2 == 0:
            texto_pixel(t, 18, 0, EXCLAMACAO, AMARELO)
    elif emocao == "thinking":
        olhos(t, "normal", dx=1, dy=-1); boca(t, "reta")
        for i in range(f % 4):
            t.px(15 + i * 2, 1, CINZA)
    elif emocao == "winking":
        olhos(t, "piscando" if f < 2 else "normal"); boca(t, "sorriso")
    elif emocao == "cool":
        olhos(t, "oculos"); boca(t, "sorriso_lado")
        if f == 1:
            t.px(14, 6, BRANCO)
    elif emocao == "relaxed":
        olhos(t, "fechado"); boca(t, "sorriso"); bochechas(t)
    elif emocao == "delicious":
        olhos(t, "feliz"); boca(t, "lingua")
        if f % 2:
            t.px(12, 10, ROSA)
    elif emocao == "kissy":
        olhos(t, "fechado"); boca(t, "beijo")
        texto_pixel(t, 14 + f, 1 - (f % 2), CORACAO, VERMELHO)
    elif emocao == "confident":
        olhos(t, "meio"); boca(t, "sorriso_lado")
        if f % 2:
            t.px(17, 2, AMARELO); t.px(16, 3, AMARELO); t.px(18, 3, AMARELO); t.px(17, 4, AMARELO)
    elif emocao == "sleepy":
        olhos(t, "fechado"); boca(t, "o" if f % 2 else "reta")
        texto_pixel(t, 15, 0 if f < 2 else -1, Z, CINZA)
    elif emocao == "silly":
        olhos(t, "doido"); boca(t, "lingua")
    elif emocao == "robot_2":            # iniciando: olhos normais e pontinhos carregando
        olhos(t, "normal"); boca(t, "reta")
        for i in range(3):
            t.px(7 + i * 3, 17, AMARELO if i == f % 3 else CINZA)  # 2 px de grade (12 px) abaixo das pernas
    elif emocao == "conectando":         # sinal de Wi-Fi acendendo por partes, olhando para os lados
        olhos(t, "normal", dx=(-1, 0, 1, 0, -1, 0)[f % 6]); boca(t, "reta")
        acesas = f % 5                   # 0..4 partes acesas
        partes = [[(9, 3, 2, 1)], [(7, 2, 1, 1), (8, 1, 4, 1), (12, 2, 1, 1)],
                  [(5, 1, 1, 2), (6, 0, 1, 1), (13, 0, 1, 1), (14, 1, 1, 2)]]
        for i, peças in enumerate(partes):
            for x, y, w, h in peças:
                t.px(x, y, AMARELO if i < acesas else CINZA_ESCURO, w, h)
    elif emocao == "ouvindo":            # de fone de ouvido, olhos grandes e atentos
        FONE, FONE_CLARO = (45, 45, 55), (95, 95, 110)
        t.px(4, 2, FONE, 12, 1); t.px(5, 1, FONE_CLARO, 10, 1)        # arco sobre a cabeça
        t.px(3, 2, FONE, 1, 2); t.px(16, 2, FONE, 1, 2)                # hastes
        for x in (0, 17):                                              # conchas nas laterais
            t.px(x, 3, FONE, 3, 6)
            t.px(x + (0 if x == 0 else 2), 4, FONE_CLARO, 1, 4)        # acolchoado
            t.px(x + 1, 5, AZUL if f % 3 != 2 else AZUL_ESCURO, 1, 2)  # luz pulsando
        olhar = (0, 0, 1, 1, -1, -1)[f % 6]                           # olha de um lado para o outro
        if f % 6 == 5:
            olhos(t, "fechado")                                        # pisca de vez em quando
        else:
            for x in (5 + olhar, 12 + olhar):                          # olhos grandes com brilho
                t.px(x, 5, OLHO, 3, 3); t.px(x + (1 if olhar >= 0 else 0), 5, BRANCO)
        boca(t, "o" if f % 3 == 1 else "reta")
    elif emocao == "falando":            # boca abrindo e fechando no ritmo da voz
        olhos(t, "feliz" if f in (2, 3) else "normal")
        abertura = (0, 3, 1, 2, 0, 2)[f % 6]
        if abertura == 0:
            boca(t, "sorriso")
        else:
            t.px(8, 10, OLHO, 4, abertura)
            if abertura > 1:
                t.px(9, 10 + abertura - 1, VERMELHO, 2, 1)
        if f % 2:
            t.px(17, 2, BRANCO); t.px(18, 1, BRANCO)
    elif emocao == "choque":             # treme, pisca em "raio-X" e solta faíscas
        raio_x = f in (1, 3)
        t = Tela(dy=(0, -1, 1, -1, 1, 0)[f % 6] * PX // 2)
        t.ox += (0, 2, -2, 3, -3, 1)[f % 6]
        if raio_x:                       # corpo apagado com o "esqueleto" claro
            corpo(t, CINZA_ESCURO)
            for y in (5, 7, 9, 11):
                t.px(5, y, BRANCO, 10, 1)
            t.px(9, 4, BRANCO, 2, 9)
            t.px(6, 6, BRANCO, 2, 2); t.px(12, 6, BRANCO, 2, 2)
        else:
            corpo(t, LARANJA, bracos_y=5 if f % 2 == 0 else 6)
            olhos(t, "grande"); boca(t, "aberta")
        for i, (x, y) in enumerate(((0, 1), (18, 0), (19, 9), (0, 11), (17, 14))):
            if (i + f) % 2 == 0:         # faíscas em zigue-zague
                t.px(x, y, AMARELO); t.px(x + 1, y + 1, AMARELO); t.px(x, y + 2, AMARELO)
        return t.img
    elif emocao == "codando":            # atrás de um terminal: linhas de código aparecendo
        olhos(t, "normal", dy=1)                         # olhando para a tela
        t.px(2, 10, CINZA_ESCURO, 16, 6)                 # monitor na frente da parte de baixo
        t.px(3, 11, OLHO, 14, 4)                         # tela escura
        cores = [VERDE, LARANJA, AZUL, AMARELO]
        linhas = min(4, f % 6)
        for k in range(linhas):                          # uma linha nova por quadro
            t.px(4 + (k % 2) * 2, 11 + k, cores[k], (9, 6, 10, 5)[k], 1)
        if f % 2:
            t.px(5 + (linhas % 2) * 2 + (9, 6, 10, 5)[min(linhas, 3)] , 11 + min(linhas, 3), BRANCO)  # cursor
        t.px(1 if f % 2 else 2, 15, LARANJA, 2, 1); t.px(17 if f % 2 == 0 else 16, 15, LARANJA, 2, 1)  # mãos
        return t.img
    elif emocao == "teclando":           # batendo rápido no teclado, teclas acendendo
        t = Tela(dy=(0, -1)[f % 2] * PX // 3)
        corpo(t, bracos_y=None)
        olhos(t, "meio" if f % 3 else "normal", dy=1); boca(t, "reta")
        t.px(2, 13, CINZA_ESCURO, 16, 2)                 # teclado
        for k in range(7):
            t.px(3 + k * 2, 13, CINZA if (k + f) % 3 else AMARELO, 1, 1)
        t.px(1 if f % 2 else 3, 11 if f % 2 else 12, LARANJA, 2, 2)    # braços alternando
        t.px(16 if f % 2 else 15, 12 if f % 2 else 11, LARANJA, 2, 2)
        if f % 3 == 0:
            t.px(17, 2, CINZA); t.px(18, 1, CINZA)       # "tec tec"
        return t.img
    elif emocao == "warning":
        olhos(t, "grande"); boca(t, "triste")
        if f % 2 == 0:
            texto_pixel(t, 18, 0, EXCLAMACAO, VERMELHO)
    elif emocao == "staticstate":
        olhos(t, "normal")
    elif emocao == "confused":
        olhos(t, "confuso"); boca(t, "reta")
        texto_pixel(t, 16, -1 + (f % 2), INTERROGACAO, AMARELO)
    return t.img


def para_gif(quadros, caminho, duracoes):
    # Fundo transparente: índice 0 da paleta reservado para a transparência
    pal = []
    for q in quadros:
        fundo = Image.new("RGBA", q.size, (0, 0, 0, 0))
        pal.append(Image.alpha_composite(fundo, q))
    convertidos = []
    for q in pal:
        alfa = q.getchannel("A")
        p = q.convert("RGB").convert("P", palette=Image.ADAPTIVE, colors=255)
        p = p.point(lambda i: i + 1)
        p.putpalette([0, 0, 0] + (p.getpalette() or [])[: 255 * 3])
        mascara = Image.eval(alfa, lambda a: 255 if a < 128 else 0)
        p.paste(0, mask=mascara)
        p.info["transparency"] = 0
        convertidos.append(p)
    convertidos[0].save(caminho, save_all=True, append_images=convertidos[1:], duration=duracoes,
                        loop=0, transparency=0, disposal=2, optimize=False)


def main():
    SAIDA.mkdir(exist_ok=True)
    previa = Image.new("RGBA", (8 * 140, -(-len(EMOCOES) // 8) * 160), (24, 24, 28, 255))
    dp = ImageDraw.Draw(previa)
    for i, emocao in enumerate(EMOCOES):
        n = 6 if emocao in ESTADOS else 4
        quadros = [quadro(emocao, f) for f in range(n)]
        duracoes = ([1400, 200, 1400, 150] if emocao == "neutral"
                    else [ESTADOS[emocao]] * n if emocao in ESTADOS else [400] * 4)
        para_gif(quadros, SAIDA / f"{emocao}.gif", duracoes)
        x, y = (i % 8) * 140 + 6, (i // 8) * 160 + 6
        previa.alpha_composite(quadros[0], (x, y))
        dp.text((x + 4, y + 132), emocao, fill=(220, 220, 220, 255))
    previa.save(AQUI / "previa.png")
    print(f"{len(EMOCOES)} GIFs em {SAIDA}")


if __name__ == "__main__":
    main()

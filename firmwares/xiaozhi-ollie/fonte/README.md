# Fontes

Fontes LVGL (`.c`) geradas com [lv_font_conv](https://github.com/lvgl/lv_font_conv), 4 bpp. O `aplicar_patches.py` copia os `.c` para a placa. Os `.ttf` não vão para o repositório; os comandos abaixo os baixam.

| Arquivo | Fonte | Uso |
|---|---|---|
| `font_noto_sans_pt_24.c` | Noto Sans Regular 24 px | Fala na tela e títulos (padrão) |
| `font_jetbrains_mono_pt_22.c` | JetBrains Mono Medium 22 px | Mesmo uso, com a opção **Fonte: JetBrains Mono** das Configurações |
| `font_jetbrains_mono_pt_18.c` | JetBrains Mono Medium 18 px | Textos menores com a Mono (no lugar da Noto Sans 20 do XiaoZhi) |
| `font_ollie_logo_88.c` | Inter Black 88 px, só `O l i e` | Logo da tela de abertura (terminais retos, sem arredondamentos) |

Todas usam a licença SIL Open Font License 1.1 ([Noto](https://github.com/notofonts/latin-greek-cyrillic), [JetBrains Mono](https://github.com/JetBrains/JetBrainsMono), [Inter](https://github.com/rsms/inter)).

## Como gerar de novo

```sh
# Caracteres do português: ASCII, Latin-1, travessões, aspas curvas, marcador, reticências e euro
R="0x20-0x7e,0xa0-0xff,0x2013-0x2014,0x2018-0x201d,0x2022,0x2026,0x20ac"
OPC="--bpp 4 --format lvgl --no-compress --lv-include lvgl.h"

curl -sSLO https://raw.githubusercontent.com/notofonts/notofonts.github.io/main/fonts/NotoSans/hinted/ttf/NotoSans-Regular.ttf
npx lv_font_conv --font NotoSans-Regular.ttf --size 24 $OPC -r $R -o font_noto_sans_pt_24.c

curl -sSLO https://raw.githubusercontent.com/JetBrains/JetBrainsMono/master/fonts/ttf/JetBrainsMono-Medium.ttf
for s in 22 18; do
  npx lv_font_conv --font JetBrainsMono-Medium.ttf --size $s $OPC -r $R \
    --lv-font-name font_jetbrains_mono_pt_$s -o font_jetbrains_mono_pt_$s.c
done

# Inter só existe como fonte variável: fixa o peso 900 (Black) e o tamanho óptico de exibição antes de converter
curl -sSL -o Inter-var.ttf "https://raw.githubusercontent.com/google/fonts/main/ofl/inter/Inter%5Bopsz,wght%5D.ttf"
uv run --with fonttools python -m fontTools.varLib.instancer Inter-var.ttf wght=900 opsz=32 -o Inter-Black.ttf
npx lv_font_conv --font Inter-Black.ttf --size 88 $OPC --symbols "Olie" \
  --lv-font-name font_ollie_logo_88 -o font_ollie_logo_88.c
```

Se mudar o texto do logo, inclua as letras novas em `--symbols`.

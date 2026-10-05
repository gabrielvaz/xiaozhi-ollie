// Idioma do Ollie: escolhido no build (compilar.sh --idioma pt-BR|en-US|zh-CN|es-ES), que liga um CONFIG_LANGUAGE_*.
// Todo texto que aparece na tela ou é falado usa TR("português", "English", "中文", "español").
// Só o texto do idioma escolhido entra no binário.
#pragma once

#include <sdkconfig.h>

#if defined(CONFIG_LANGUAGE_EN_US)
#define TR(pt, en, zh, es) (en)
#define OLLIE_IDIOMA "en-US"
#elif defined(CONFIG_LANGUAGE_ZH_CN)
#define TR(pt, en, zh, es) (zh)
#define OLLIE_IDIOMA "zh-CN"
#define OLLIE_IDIOMA_CJK 1  // as fontes próprias não têm ideogramas: usa a fonte Noto CJK do tema (fontes_watcher.h)
#elif defined(CONFIG_LANGUAGE_ES_ES)
#define TR(pt, en, zh, es) (es)
#define OLLIE_IDIOMA "es-ES"
#else
#define TR(pt, en, zh, es) (pt)
#define OLLIE_IDIOMA "pt-BR"
#endif

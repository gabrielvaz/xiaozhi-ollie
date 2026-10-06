/* LVGL do simulador: mesma versão (9.5) e profundidade de cor (RGB565) do firmware */
#ifndef LV_CONF_H
#define LV_CONF_H
#define LV_COLOR_DEPTH 16
#define LV_USE_STDLIB_MALLOC LV_STDLIB_CLIB
#define LV_USE_STDLIB_STRING LV_STDLIB_CLIB
#define LV_USE_STDLIB_SPRINTF LV_STDLIB_CLIB
#define LV_USE_OS LV_OS_NONE
#define LV_DEF_REFR_PERIOD 33
#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_USE_FLEX 1
#define LV_USE_GRID 1
#define LV_USE_QRCODE 1
#define LV_USE_SNAPSHOT 1
#define LV_USE_DRAW_SW 1
#define LV_DRAW_SW_COMPLEX 1
#define LV_USE_MATRIX 1
#define LV_USE_FLOAT 1
#define LV_USE_ARC 1
#define LV_USE_SPINNER 1
#define LV_USE_IMAGE 1
#define LV_USE_LABEL 1
#define LV_LABEL_LONG_TXT_HINT 1
#define LV_USE_THEME_DEFAULT 1
#define LV_THEME_DEFAULT_DARK 0
#define LV_TXT_ENC LV_TXT_ENC_UTF8
#endif

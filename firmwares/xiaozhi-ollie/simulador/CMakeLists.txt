# Simulador da tela do Watcher: LVGL 9.5 (o mesmo do firmware) compilado para o Mac
cmake_minimum_required(VERSION 3.16)
project(simulador_watcher C CXX)
set(CMAKE_CXX_STANDARD 17)
set(XZ ${CMAKE_CURRENT_SOURCE_DIR}/../xiaozhi-esp32)
set(LVGL ${XZ}/managed_components/lvgl__lvgl)
set(FONTES ${XZ}/managed_components/78__xiaozhi-fonts)

file(GLOB_RECURSE LVGL_SRC ${LVGL}/src/*.c)
add_library(lvgl STATIC ${LVGL_SRC})
target_include_directories(lvgl PUBLIC ${LVGL} ${LVGL}/src ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(lvgl PUBLIC LV_CONF_INCLUDE_SIMPLE)

add_executable(simulador
    main.cc
    ${XZ}/main/display/lvgl_display/gif/gifdec.c
    ${XZ}/main/display/lvgl_display/gif/lvgl_gif.cc
    ${XZ}/main/display/lvgl_display/emoji_collection.cc
    ${FONTES}/src/font_noto_sans_basic_30_4.c
    ${FONTES}/src/font_noto_sans_basic_20_4.c
    ${FONTES}/src/font_material_symbols_20_4.c
    ${FONTES}/src/font_material_symbols_30_4.c
    ${FONTES}/src/font_material_symbols_16_4.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../fonte/font_noto_sans_pt_24.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../fonte/font_ollie_logo_88.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../fonte/font_jetbrains_mono_pt_22.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../fonte/font_jetbrains_mono_pt_18.c
)
target_include_directories(simulador PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/stubs ${XZ}/main/display/lvgl_display ${FONTES}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../placa)
target_link_libraries(simulador lvgl)

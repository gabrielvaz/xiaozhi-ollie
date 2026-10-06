"""Patches do firmware XiaoZhi v2.5.0 para o SenseCAP Watcher (Xiaozhi Ollie).

1. Ajusta textos pt-BR da interface (language.json).
2. Traduz para português as ferramentas MCP da câmera (sscma_camera.cc), lidas pelo modelo.
3. Portal de Wi-Fi: acrescenta pt-BR (o original só tem pt-PT) na cópia local do esp-wifi-connect 3.3.1.
4. Usa essa cópia local no lugar da versão do registro (override_path).
5. Embute o endereço OTA do servidor do Mac (lido de servidor/.env) na configuração da placa.

Idempotente. Uso: python aplicar_patches.py
"""

import json
import re
import sys
from pathlib import Path

AQUI = Path(__file__).resolve().parent
XZ = AQUI / "xiaozhi-esp32"
WIFI = AQUI / "esp-wifi-connect"
ENV = AQUI.parents[1] / "servidor" / ".env"
problemas: list[str] = []

# Parte sempre dos originais: os patches são aplicados em sequência sobre o código limpo,
# então um patch pode mexer no resultado de outro sem duplicar nada ao rodar de novo.
import subprocess
for repo in (XZ, WIFI):
    subprocess.run(["git", "-C", str(repo), "checkout", "--", "."], check=True)
    subprocess.run(["git", "-C", str(repo), "clean", "-fdq", "--", "main/boards/sensecap-watcher", "assets"], check=False)


def trocar(arq: Path, antigo: str, novo: str) -> None:
    texto = arq.read_text(encoding="utf-8")
    if novo in texto:
        return
    if antigo in texto:
        arq.write_text(texto.replace(antigo, novo), encoding="utf-8")
    elif novo not in texto:
        problemas.append(f"{arq.relative_to(AQUI)}: não achei {antigo[:50]!r}")


# 1. Textos da interface -------------------------------------------------------
lang = XZ / "main/assets/locales/pt-BR/language.json"
dados = json.loads(lang.read_text(encoding="utf-8"))
dados["strings"].update({
    "LISTENING": "Ouvindo...",
    "STANDBY": "Pronto",
    "SERVER_NOT_FOUND": "Procurando o servidor...",
    "SERVER_NOT_CONNECTED": "Sem conexão com o servidor, tente mais tarde",
    "SERVER_ERROR": "Falha no envio, confira a rede",
    "CONNECT_TO_HOTSPOT": "No celular, conecte-se à rede ",
    "ACCESS_VIA_BROWSER": " e abra no navegador ",
    "HELLO_MY_FRIEND": "Olá!",
    "RTC_MODE_OFF": "Cancelamento de eco desligado",
    "RTC_MODE_ON": "Cancelamento de eco ligado",
    "VOLUME": "Volume",
})
lang.write_text(json.dumps(dados, ensure_ascii=False, indent=4) + "\n", encoding="utf-8")

# 2. Ferramentas da câmera em português ----------------------------------------
cam = XZ / "main/boards/sensecap-watcher/sscma_camera.cc"
for antigo, novo in [
    ('"获取当前视觉模型检测的参数配置信息。\\n"', '"Lê a configuração atual da detecção visual (modelo de IA da câmera).\\n"'),
    ('"返回结果包含：\\n"', '"O resultado traz:\\n"'),
    ('"  `threshold`: 检测置信度阈值 (0-100)，低于此值的检测结果将被忽略；\\n"', '"  `threshold`: confiança mínima (0-100); detecções abaixo disso são ignoradas;\\n"'),
    ('"  `interval`: 触发对话后的冷却时间(秒)，防止频繁打断；\\n"', '"  `interval`: tempo de espera (s) depois de uma conversa disparada, para não interromper toda hora;\\n"'),
    ('"  `duration`: 持续检测确认时间(秒)；\\n"', '"  `duration`: tempo (s) que o alvo precisa ficar visível para confirmar;\\n"'),
    ('"  `target`: 当前关注的检测目标索引。"', '"  `target`: índice do alvo monitorado."'),
    ('"配置视觉模型检测参数。当用户希望调整检测灵敏度、频率或特定目标时使用。\\n"', '"Ajusta a detecção visual. Use quando o usuário quiser mudar sensibilidade, frequência ou alvo.\\n"'),
    ('"参数(均为可选，未提供的参数将保持当前设置不变)：\\n"', '"Parâmetros (todos opcionais; os omitidos ficam como estão):\\n"'),
    ('"  `threshold`: 置信度阈值 (0-100)。提高此值可减少误报，但可能漏检；\\n"', '"  `threshold`: confiança mínima (0-100); subir reduz falsos alarmes, mas pode perder detecções;\\n"'),
    ('"  `interval`: 冷却时间(秒)。设置对话结束后多久内不再触发检测；\\n"', '"  `interval`: tempo de espera (s) depois de uma conversa;\\n"'),
    ('"  `duration`: 持续检测时间(秒)。\\n"', '"  `duration`: tempo de confirmação (s).\\n"'),
    ('"  `target`: 设置检测目标的索引 ID。"', '"  `target`: índice do alvo."'),
    ('"控制视觉推理(摄像头检测)功能的开启与关闭，或查询当前状态。\\n"', '"Liga, desliga ou consulta a detecção visual da câmera.\\n"'),
    ('"当用户指令涉及\'开启/关闭推理\'、\'开始/停止检测\'时使用。\\n"', '"Use quando o usuário pedir para ligar ou desligar a detecção.\\n"'),
    ('"参数：\\n"', '"Parâmetro:\\n"'),
    ('"  `enable`: (可选) 整数。1=开启推理，0=关闭推理。若省略则返回当前开关状态。"', '"  `enable`: (opcional) 1 liga, 0 desliga; sem ele, retorna o estado atual."'),
]:
    trocar(cam, antigo, novo)

# 3. Portal de Wi-Fi em pt-BR ----------------------------------------------------
html = WIFI / "assets/wifi_configuration.html"
PT_BR = """            'pt-BR': {
                title: 'Configuração de rede',
                saved_wifi: 'Redes salvas',
                new_wifi: 'Nova rede Wi-Fi',
                password: 'Senha:',
                connect: 'Conectar',
                connecting: 'Conectando...',
                select_wifi: 'Escolha uma rede Wi-Fi de 2,4 GHz na lista abaixo:',
                select_wifi_5g: 'Escolha uma rede Wi-Fi na lista abaixo:',
                scanning: 'Procurando redes...',
                wifi_tab: 'Wi-Fi',
                advanced_tab: 'Avançado',
                ota_url: 'Endereço do servidor (OTA):',
                max_tx_power: 'Potência máxima do Wi-Fi:',
                remember_bssid: 'Lembrar o BSSID ao conectar',
                sleep_mode: 'Ativar modo de economia',
                save: 'Salvar',
                config_success: 'Configuração concluída!'
            },
"""
texto = html.read_text(encoding="utf-8")
if "'pt-BR': {" not in texto:
    texto = texto.replace("            'pt-PT': {", PT_BR + "            'pt-PT': {", 1)
texto = texto.replace("'pt-BR': 'pt-PT'", "'pt-BR': 'pt-BR'")
texto = texto.replace("'pt': 'pt-PT'", "'pt': 'pt-BR'")
if '<option value="pt-BR">' not in texto:
    texto = texto.replace('<option value="pt-PT">Português</option>',
                          '<option value="pt-BR">Português (Brasil)</option>\n            <option value="pt-PT">Português (Portugal)</option>')
html.write_text(texto, encoding="utf-8")
for marca in ("'pt-BR': {", '<option value="pt-BR">', "'pt-BR': 'pt-BR'"):
    if marca not in texto:
        problemas.append(f"portal: faltou {marca}")

# 4. Usar a cópia local do esp-wifi-connect ------------------------------------
manifesto = XZ / "main/idf_component.yml"
trocar(manifesto, "  78/esp-wifi-connect: ~3.3.1\n",
       '  78/esp-wifi-connect:\n    version: ~3.3.1\n    override_path: "../../esp-wifi-connect"\n')

# 5. Endereço do servidor embutido ---------------------------------------------
env = dict(re.findall(r"^([A-Z_]+)=(.*)$", ENV.read_text(), re.M)) if ENV.exists() else {}
if env.get("HOST_PUBLICO") and env.get("SEGREDO"):
    ota = f"https://{env['HOST_PUBLICO']}/{env['SEGREDO']}/http/xiaozhi/ota/"
    cfg = XZ / "main/boards/sensecap-watcher/config.json"
    placa = json.loads(cfg.read_text(encoding="utf-8"))
    extra = placa["builds"][0]["sdkconfig_append"]
    extra[:] = [l for l in extra if not l.startswith("CONFIG_OTA_URL=")] + [f'CONFIG_OTA_URL="{ota}"']
    # Diagnóstico: tarefa que trava o processador por 10 s reinicia o aparelho (o motivo e o rastro vão ao Mac)
    if "CONFIG_ESP_TASK_WDT_PANIC=y" not in extra:
        extra.append("CONFIG_ESP_TASK_WDT_PANIC=y")
    cfg.write_text(json.dumps(placa, ensure_ascii=False, indent=4) + "\n", encoding="utf-8")
else:
    problemas.append("servidor/.env sem HOST_PUBLICO/SEGREDO: endereço OTA não embutido")

# 6. Mascote: Clawd (Claude Code) no lugar dos emojis ---------------------------
#    A coleção vem de mascote-clawd/emocoes (gerada por mascote-clawd/gerar.py).
ativos = XZ / "scripts/build_default_assets.py"
trocar(ativos, "    # Special handling for otto-gif collection\n",
       "    # Coleção local por caminho absoluto (mascote Clawd)\n"
       "    if os.path.isabs(default_emoji_collection) and os.path.isdir(default_emoji_collection):\n"
       "        return default_emoji_collection\n\n"
       "    # Special handling for otto-gif collection\n")
cmake = XZ / "main/CMakeLists.txt"
trocar(cmake, """    set(BOARD_DIR "sensecap-watcher")
    set(BUILTIN_TEXT_FONT font_noto_sans_basic_30_4)
    set(BUILTIN_ICON_FONT font_material_symbols_20_4)
    set(DEFAULT_EMOJI_COLLECTION noto-color-emoji_128)""", f"""    set(BOARD_DIR "sensecap-watcher")
    set(BUILTIN_TEXT_FONT font_noto_sans_basic_30_4)
    set(BUILTIN_ICON_FONT font_material_symbols_20_4)
    set(DEFAULT_EMOJI_COLLECTION "{AQUI / 'mascote-clawd/emocoes'}")""")
trocar(cmake, """            "${NOTO_FONTS_PATH}/png/${DEFAULT_EMOJI_COLLECTION}/*"
        )""", """            "${NOTO_FONTS_PATH}/png/${DEFAULT_EMOJI_COLLECTION}/*"
            "${DEFAULT_EMOJI_COLLECTION}/*"
        )""")

# 7. Tela redonda: fala em várias linhas abaixo do mascote ------------------------
#    O padrão é uma faixa de uma linha rolando de lado no rodapé.
import shutil
placa_dir = XZ / "main/boards/sensecap-watcher"
# Fontes geradas com lv_font_conv (ver fonte/README.md): fala, Mono das Configurações e logo da abertura
for fonte_c in ("font_noto_sans_pt_24.c", "font_jetbrains_mono_pt_22.c", "font_jetbrains_mono_pt_18.c",
                "font_ollie_logo_88.c"):
    shutil.copy(AQUI / "fonte" / fonte_c, placa_dir / fonte_c)
placa = placa_dir / "sensecap_watcher.cc"
trocar(placa, '#include <iot_knob.h>\n', '#include <iot_knob.h>\n\n#include "abertura_watcher.h"  // tela de abertura com o logo\n#include "fontes_watcher.h"    // Noto Sans ou JetBrains Mono (Configurações)\n#include "layout_mascote.h"    // onde ficam o Clawd e o texto\n#include "anel_volume.h"       // anel do volume na borda\n#include "tela_sem_wifi.h"     // modo de configuração de Wi-Fi\n')
trocar(placa, """            lv_obj_set_style_pad_bottom(bottom_bar_, 30, 0);
            lv_obj_set_width(chat_message_label_, LV_HOR_RES * 0.75); // 限制宽度，避免文字贴边
        }
};""", """            AplicarLayoutRedondo();
            AberturaWatcher::Mostrar();
        }

        // Fala em streaming na parte larga do círculo (412x412): 3 linhas visíveis, rola para baixo.
        // A altura do Clawd e do texto vem de placa/layout_mascote.h (a mesma regra em todos os fluxos)
        static constexpr int kTextoLargura = 290;
        static constexpr int kLinhasVisiveis = 3;
        static constexpr uint32_t kTickFalaMs = 66;  // ~15 caracteres por segundo, o ritmo medido da voz

        std::string fala_;            // texto completo do turno atual
        size_t fala_exibida_ = 0;     // bytes de fala_ já na tela
        std::string papel_atual_;
        lv_timer_t* timer_fala_ = nullptr;

        void AplicarLayoutRedondo() {
            if (bottom_bar_ == nullptr || chat_message_label_ == nullptr) {
                return;
            }
            lv_obj_set_size(bottom_bar_, kTextoLargura, lv_font_get_line_height(Fontes::Grande()) * kLinhasVisiveis + 4);
            lv_obj_set_style_bg_opa(bottom_bar_, LV_OPA_TRANSP, 0);
            lv_obj_set_style_pad_all(bottom_bar_, 0, 0);
            lv_obj_add_flag(bottom_bar_, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_scroll_dir(bottom_bar_, LV_DIR_VER);
            lv_obj_set_scrollbar_mode(bottom_bar_, LV_SCROLLBAR_MODE_OFF);
            lv_obj_set_width(chat_message_label_, kTextoLargura);
            lv_obj_set_height(chat_message_label_, LV_SIZE_CONTENT);
            lv_label_set_long_mode(chat_message_label_, LV_LABEL_LONG_WRAP);  // quebra linha (o modo de rolagem não quebra)
            lv_obj_set_style_text_font(chat_message_label_, Fontes::Grande(), 0);
            lv_obj_set_style_text_line_space(chat_message_label_, 0, 0);
            lv_obj_set_style_text_align(chat_message_label_, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_align(chat_message_label_, LV_ALIGN_TOP_MID, 0, 0);
            // Dia, data e hora menores no topo (a fonte do tema é grande para o arco da tela)
            if (status_label_ != nullptr) {
                lv_obj_set_style_text_font(status_label_, Fontes::Pequena(), 0);
            }
            // Volume e outras notificações do topo seguem a fonte escolhida em Ajustes
            if (notification_label_ != nullptr) {
                lv_obj_set_style_text_font(notification_label_, Fontes::Pequena(), 0);
            }
            PosicionarMascote();  // a altura da linha muda com a fonte
        }

        int AlturaCaixaTexto() const {
            return lv_font_get_line_height(Fontes::Grande()) * kLinhasVisiveis + 4;
        }

        // Clawd e texto como um bloco centrado na vertical; sem texto, o Clawd no centro exato
        void PosicionarMascote() {
            LayoutMascote::Aplicar(emoji_box_, emoji_image_, bottom_bar_, chat_message_label_, papel_atual_,
                                   hide_subtitle_, AlturaCaixaTexto());
        }

        virtual void SetHideSubtitle(bool hide) override {
            SpiLcdDisplay::SetHideSubtitle(hide);
            DisplayLockGuard lock(this);
            PosicionarMascote();  // modo só voz: sem texto, o Clawd volta ao centro
        }

        void RolarParaFim() {
            lv_obj_update_layout(bottom_bar_);
            int32_t falta = lv_obj_get_scroll_bottom(bottom_bar_);
            if (falta > 0) {
                lv_obj_scroll_by(bottom_bar_, 0, -falta, LV_ANIM_OFF);
            }
        }

        static void TickFala(lv_timer_t* timer) {
            static_cast<CustomLcdDisplay*>(lv_timer_get_user_data(timer))->AvancarFala();
        }

        // Mostra mais um pedaço da fala; acelera se o texto acumulado estiver muito à frente
        void AvancarFala() {
            if (chat_message_label_ == nullptr || fala_exibida_ >= fala_.size()) {
                if (timer_fala_ != nullptr) {
                    lv_timer_pause(timer_fala_);
                }
                return;
            }
            // Acompanha a voz: enquanto fala, só avança com o áudio tocando (espera o início e as pausas)
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateSpeaking && app.VozLigada() &&
                app.GetAudioService().IsPlaybackIdle()) {
                return;
            }
            size_t atraso = fala_.size() - fala_exibida_;
            int passos = app.GetDeviceState() != kDeviceStateSpeaking ? 8 : (atraso > 200 ? 2 : 1);
            for (int i = 0; i < passos && fala_exibida_ < fala_.size(); i++) {
                fala_exibida_++;
                while (fala_exibida_ < fala_.size() &&
                       (static_cast<unsigned char>(fala_[fala_exibida_]) & 0xC0) == 0x80) {
                    fala_exibida_++;  // não corta caracteres acentuados (UTF-8) ao meio
                }
            }
            lv_label_set_text(chat_message_label_, fala_.substr(0, fala_exibida_).c_str());
            RolarParaFim();
        }

        virtual void SetChatMessage(const char* role, const char* content) override {
            DisplayLockGuard lock(this);
            if (chat_message_label_ == nullptr || bottom_bar_ == nullptr) {
                return;
            }
            std::string papel = role ? role : "";
            std::string texto = content ? content : "";
            CartaoWatcher::Instancia().RegistrarConversa(papel, texto);  // registro local no microSD
            if (texto.empty()) {
                fala_.clear();
                fala_exibida_ = 0;
                papel_atual_.clear();
                if (timer_fala_ != nullptr) {
                    lv_timer_pause(timer_fala_);
                }
                lv_label_set_text(chat_message_label_, "");
                lv_obj_add_flag(bottom_bar_, LV_OBJ_FLAG_HIDDEN);
                PosicionarMascote();
                return;
            }
            if (papel == "assistant" && papel_atual_ == "assistant") {
                fala_ += " " + texto;  // próxima frase da mesma resposta
            } else {
                fala_ = texto;
                fala_exibida_ = 0;
                lv_obj_scroll_to_y(bottom_bar_, 0, LV_ANIM_OFF);
            }
            papel_atual_ = papel;  // "saudacao" (espera), "carregando" ("Verificando atualização") ou conversa
            if (papel == "assistant") {
                if (timer_fala_ == nullptr) {
                    timer_fala_ = lv_timer_create(TickFala, kTickFalaMs, this);
                }
                lv_timer_resume(timer_fala_);
            } else {
                // O que o usuário falou e avisos do sistema aparecem de uma vez
                fala_exibida_ = fala_.size();
                lv_label_set_text(chat_message_label_, fala_.c_str());
                RolarParaFim();
            }
            if (!hide_subtitle_) {
                lv_obj_remove_flag(bottom_bar_, LV_OBJ_FLAG_HIDDEN);
            }
            PosicionarMascote();
        }

        // Roda girando (volume): o Clawd mostra "volume_mais" ou "volume_menos" e o anel branco na borda mostra
        // o volume enquanto gira; 1,2 s depois do último passo o anel some e o Clawd volta para a emoção de
        // antes (ou a que chegou nesse meio-tempo)
        static constexpr uint32_t kVolumeFimMs = 1200;
        std::string emocao_atual_ = "neutral";
        int volume_sentido_ = 0;  // 0: parado; 1: aumentando; -1: diminuindo
        lv_timer_t* timer_volume_ = nullptr;
        AnelVolume anel_volume_;

        virtual void SetEmotion(const char* emotion) override {
            DisplayLockGuard lock(this);
            emocao_atual_ = emotion ? emotion : "neutral";
            if (volume_sentido_ == 0) {
                SpiLcdDisplay::SetEmotion(emotion);
            }
        }

        void MostrarVolume(bool aumentando, int volume) {
            DisplayLockGuard lock(this);
            anel_volume_.Mostrar(volume);
            int sentido = aumentando ? 1 : -1;
            if (sentido != volume_sentido_) {  // só troca o GIF quando muda o sentido (não reinicia a animação)
                volume_sentido_ = sentido;
                SpiLcdDisplay::SetEmotion(aumentando ? "volume_mais" : "volume_menos");
            }
            if (timer_volume_ == nullptr) {
                timer_volume_ = lv_timer_create(FimVolume, kVolumeFimMs, this);
            }
            lv_timer_reset(timer_volume_);
            lv_timer_resume(timer_volume_);
        }

        static void FimVolume(lv_timer_t* timer) {
            auto self = static_cast<CustomLcdDisplay*>(lv_timer_get_user_data(timer));
            lv_timer_pause(timer);
            self->volume_sentido_ = 0;
            self->anel_volume_.Esconder();
            self->SpiLcdDisplay::SetEmotion(self->emocao_atual_.c_str());
        }

        // Modo de configuração de Wi-Fi: tela própria (placa/tela_sem_wifi.h) no lugar do alerta do XiaoZhi
        TelaSemWifi* sem_wifi_ = nullptr;

        virtual bool MostrarSemWifi(const std::string& rede, const std::string& url) override {
            DisplayLockGuard lock(this);
            if (sem_wifi_ == nullptr) {
                sem_wifi_ = new TelaSemWifi(this);
            }
            sem_wifi_->Mostrar(rede, url);
            return true;
        }

        virtual void EsconderSemWifi() override {
            DisplayLockGuard lock(this);
            if (sem_wifi_ != nullptr) {
                sem_wifi_->Esconder();
            }
        }

        // Percentual da bateria ao lado do ícone (o topo do círculo é estreito: os ícones vão um pouco à esquerda)
        lv_obj_t* bateria_pct_ = nullptr;

        virtual void UpdateStatusBar(bool update_all = false) override {
            SpiLcdDisplay::UpdateStatusBar(update_all);
            int nivel = 0;
            bool carregando = false, descarregando = false;
            if (!Board::GetInstance().GetBatteryLevel(nivel, carregando, descarregando)) {
                return;
            }
            DisplayLockGuard lock(this);
            if (top_bar_ == nullptr || battery_label_ == nullptr) {
                return;
            }
            if (bateria_pct_ == nullptr) {
                auto icone = static_cast<LvglTheme*>(current_theme_)->icon_font()->font();
                int l = icone->line_height;
                lv_obj_align(network_label_, LV_ALIGN_TOP_MID, -5 * l / 2, 0);
                lv_obj_align(mute_label_, LV_ALIGN_TOP_MID, -l / 2, 0);
                lv_obj_align(battery_label_, LV_ALIGN_TOP_MID, 3 * l / 2, 0);
                lv_obj_set_style_text_font(network_label_, &font_material_symbols_16_4, 0);  // Wi-Fi menor
                bateria_pct_ = lv_label_create(top_bar_);
                lv_obj_set_style_text_font(bateria_pct_, Fontes::Pequena(), 0);
            }
            lv_obj_set_style_text_color(bateria_pct_, lv_obj_get_style_text_color(battery_label_, LV_PART_MAIN), 0);
            lv_label_set_text_fmt(bateria_pct_, "%d%%", nivel);
            lv_obj_align_to(bateria_pct_, battery_label_, LV_ALIGN_OUT_RIGHT_MID, 4, 0);
        }

        virtual void SetTheme(Theme* theme) override {
            SpiLcdDisplay::SetTheme(theme);
            DisplayLockGuard lock(this);
            AplicarLayoutRedondo();
        }
};""")

# 8. Tela inicial: dia da semana e data junto com a hora, no formato do idioma ("Seg, 05/10 · 16:20") ----
trocar(XZ / "main/display/lvgl_display/lvgl_display.cc", """                char time_str[16];
                strftime(time_str, sizeof(time_str), "%H:%M", tm_now);
                SetStatus(time_str);""", """                // Data no formato do idioma do build: "Seg, 05/10 · 16:20", "Mon, Oct 5 · 16:20", "10月5日 周一 16:20"
                char time_str[48];
                int dia_semana = tm_now->tm_wday % 7;
#if defined(CONFIG_LANGUAGE_EN_US)
                static const char* const kDiasSemana[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
                static const char* const kMeses[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                                     "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
                snprintf(time_str, sizeof(time_str), "%s, %s %d · %02d:%02d", kDiasSemana[dia_semana],
                         kMeses[tm_now->tm_mon % 12], tm_now->tm_mday, tm_now->tm_hour, tm_now->tm_min);
#elif defined(CONFIG_LANGUAGE_ZH_CN)
                static const char* const kDiasSemana[] = {"周日", "周一", "周二", "周三", "周四", "周五", "周六"};
                snprintf(time_str, sizeof(time_str), "%d月%d日 %s %02d:%02d", tm_now->tm_mon + 1, tm_now->tm_mday,
                         kDiasSemana[dia_semana], tm_now->tm_hour, tm_now->tm_min);
#else
#if defined(CONFIG_LANGUAGE_ES_ES)
                static const char* const kDiasSemana[] = {"Dom", "Lun", "Mar", "Mié", "Jue", "Vie", "Sáb"};
#else
                static const char* const kDiasSemana[] = {"Dom", "Seg", "Ter", "Qua", "Qui", "Sex", "Sáb"};
#endif
                snprintf(time_str, sizeof(time_str), "%s, %02d/%02d · %02d:%02d", kDiasSemana[dia_semana],
                         tm_now->tm_mday, tm_now->tm_mon + 1, tm_now->tm_hour, tm_now->tm_min);
#endif
                SetStatus(time_str);""")

# 9. Reset de fábrica só depois de 20 s segurando a roda (original: 10 s) ---------
trocar(placa, """            // 长按10s 恢复出厂设置: 2+0.02*400 = 10
            if (self->long_press_cnt_ > 400) {""", """            // Segurar 20 s restaura as configurações de fábrica: 2 + 0,02 * 900 = 20
            if (self->long_press_cnt_ > 900) {""")

# 10. Dois cliques na roda: abre a gaveta de apps (ou volta); três cliques fecham (placa/gaveta_watcher.h) -----------
trocar(placa, """        }, this);
    }

    void InitializeSpi() {""", """        }, this);

        // Três cliques: fecha a gaveta de qualquer tela
        static button_event_args_t tres_cliques = {};
        tres_cliques.multiple_clicks.clicks = 3;
        iot_button_register_cb(btns, BUTTON_MULTIPLE_CLICK, &tres_cliques, [](void* button_handle, void* usr_data) {
            auto self = static_cast<SensecapWatcher*>(usr_data);
            self->power_save_timer_->WakeUp();
            if (self->gaveta_ != nullptr) {
                self->gaveta_->Fechar();
            }
        }, this);

        // Dois cliques: abre a gaveta; com ela aberta, volta (tela anterior do app, gaveta, ou fecha)
        iot_button_register_cb(btns, BUTTON_DOUBLE_CLICK, nullptr, [](void* button_handle, void* usr_data) {
            auto self = static_cast<SensecapWatcher*>(usr_data);
            self->power_save_timer_->WakeUp();
            if (self->gaveta_ != nullptr) {
                self->gaveta_->AbrirOuVoltar();
            }
        }, this);
    }

    void InitializeSpi() {""")

# 11. Tema escuro como padrão (o original começa no claro) ----------------------------
#     Fixo no boot: o tema salvo na memória (gravado como "light" no 1º boot) venceria o padrão
lcd = XZ / "main/display/lcd_display.cc"
trocar(lcd, """    Settings settings("display", false);
    std::string theme_name = settings.GetString("theme", "light");""", """    // Tema escolhido em Apps > Configurações (padrão: escuro)
    Settings watcher_settings("watcher", false);
    std::string theme_name = watcher_settings.GetString("tema");
    if (theme_name.empty()) {
        theme_name = "dark";
    }""")

# 12. Gaveta de ações, navegador de sessões, gravação e avisos (placa/*.h) -------------
for arq in (AQUI / "placa").rglob("*.h"):
    destino_arq = placa_dir / arq.relative_to(AQUI / "placa")
    destino_arq.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy(arq, destino_arq)
trocar(XZ / "main/protocols/protocol.h", "    virtual bool OpenAudioChannel() = 0;",
       "    virtual bool OpenAudioChannel() = 0;\n    // Mensagem própria (ex.: {\"type\":\"reuniao\"}) pelo mesmo canal\n    bool SendJson(const std::string& json) { return SendText(json); }")
trocar(XZ / "main/application.h", "    void StopListening();",
       "    void StopListening();\n    void EnviarJson(const std::string& json);  // mensagem própria ao servidor (reunião)")
trocar(XZ / "main/application.cc", "void Application::StopListening() { xEventGroupSetBits(event_group_, MAIN_EVENT_STOP_LISTENING); }",
       """void Application::StopListening() { xEventGroupSetBits(event_group_, MAIN_EVENT_STOP_LISTENING); }

void Application::EnviarJson(const std::string& json) {
    Schedule([this, json]() {
        if (protocol_) {
            protocol_->SendJson(json);
        }
    });
}""")
trocar(placa, '#include <iot_knob.h>\n', '#include <iot_knob.h>\n#include "registro_apps.h"\n')
trocar(placa, """    static SensecapWatcher* instance_;""", """    static SensecapWatcher* instance_;
    GavetaWatcher* gaveta_ = nullptr;""")
trocar(placa, """        InitializeCamera();
    }""", """        InitializeCamera();
        if (IoExpanderGetLevel(BSP_SD_GPIO_DET) == 0) {  // cartão presente
            CartaoWatcher::Instancia().Montar(BSP_SD_SPI_NUM, BSP_SD_SPI_CS);
        }
        gaveta_ = new GavetaWatcher(display_);
        gaveta_->Contexto().definir_tela_apaga_s = [this](int s) { power_save_timer_->SetSecondsToSleep(s); };
        gaveta_->Contexto().definir_desliga_s = [this](int s) { power_save_timer_->SetSecondsToShutdown(s); };
        RegistrarApps(*gaveta_);
        gaveta_->Iniciar();
    }""")
trocar(placa, """    void OnKnobRotate(bool clockwise) {
        auto codec = GetAudioCodec();""", """    void OnKnobRotate(bool clockwise) {
        if (gaveta_ != nullptr && gaveta_->Girar(clockwise)) {
            return;  // painel aberto: a roda navega
        }
        auto codec = GetAudioCodec();""")
trocar(placa, """            app.ToggleChatState();""", """            if (self->gaveta_ != nullptr && self->gaveta_->Clicar()) {
                return;  // painel aberto: o clique escolhe
            }
            app.ToggleChatState();""")

# 13. microSD e gravação local: gancho de áudio, microfone ligado durante a reunião, cartão na placa
app_h, app_cc = XZ / "main/application.h", XZ / "main/application.cc"
trocar(app_h, "    void EnviarJson(const std::string& json);  // mensagem própria ao servidor (reunião)",
       """    void EnviarJson(const std::string& json);  // mensagem própria ao servidor (reunião)
    // Reunião: cópia de cada pacote de áudio (microSD) e microfone ligado mesmo sem conexão
    void DefinirGanchoAudio(std::function<void(const std::vector<uint8_t>&)> gancho) { gancho_audio_ = std::move(gancho); }
    void GravacaoLocal(bool ligada);
    std::function<void(const std::vector<uint8_t>&)> gancho_audio_;
    bool gravacao_local_ = false;""")
trocar(app_cc, """            while (auto packet = audio_service_.PopPacketFromSendQueue()) {
                if (protocol_ && !protocol_->SendAudio(std::move(packet))) {""", """            while (auto packet = audio_service_.PopPacketFromSendQueue()) {
                if (gancho_audio_) {
                    gancho_audio_(packet->payload);  // cópia da reunião no microSD
                }
                if (protocol_ && !protocol_->SendAudio(std::move(packet))) {""")
trocar(app_cc, """                    while (audio_service_.PopPacketFromSendQueue())
                        ;
                    break;""", """                    while (auto resto = audio_service_.PopPacketFromSendQueue()) {
                        if (gancho_audio_) {
                            gancho_audio_(resto->payload);  // sem conexão, a reunião segue no cartão
                        }
                    }
                    break;""")
trocar(app_cc, """            audio_service_.EnableVoiceProcessing(false);
            audio_service_.EnableWakeWordDetection(true);
            break;""", """            audio_service_.EnableVoiceProcessing(gravacao_local_);  // reunião continua gravando sem conexão
            audio_service_.EnableWakeWordDetection(!gravacao_local_);
            break;""")
trocar(app_cc, """void Application::EnviarJson(const std::string& json) {""", """void Application::GravacaoLocal(bool ligada) {
    Schedule([this, ligada]() {
        gravacao_local_ = ligada;
        if (GetDeviceState() == kDeviceStateIdle) {
            audio_service_.EnableWakeWordDetection(!ligada);
            audio_service_.EnableVoiceProcessing(ligada);
        }
    });
}

void Application::EnviarJson(const std::string& json) {""")
trocar(XZ / "main/CMakeLists.txt", """if(CONFIG_BOARD_TYPE_ESP32_P4_FUNCTION_EV_BOARD)
    list(APPEND MAIN_PRIV_REQUIRES_EXTRA""", """if(CONFIG_BOARD_TYPE_SEEED_STUDIO_SENSECAP_WATCHER)
    list(APPEND MAIN_PRIV_REQUIRES_EXTRA esp_driver_sdspi sdmmc)  # microSD do Watcher
endif()
if(CONFIG_BOARD_TYPE_ESP32_P4_FUNCTION_EV_BOARD)
    list(APPEND MAIN_PRIV_REQUIRES_EXTRA""")


# 14. Tela: tempos das Configurações, tela apaga (brilho 0) e a roda também acorda ------------------
trocar(XZ / "main/boards/common/power_save_timer.h", "    void WakeUp();",
       "    void WakeUp();\n    void SetSecondsToSleep(int s) { seconds_to_sleep_ = s; ticks_ = 0; }      // -1 = nunca\n"
       "    void SetSecondsToShutdown(int s) { seconds_to_shutdown_ = s; ticks_ = 0; }")
trocar(placa, """        power_save_timer_ = new PowerSaveTimer(-1, 60, 300);""",
       """        Settings ajustes("watcher", false);  // Apps > Configurações
        power_save_timer_ = new PowerSaveTimer(-1, ajustes.GetInt("tela_s", 60), ajustes.GetInt("desliga_s", 300));""")
trocar(placa, """            GetDisplay()->SetPowerSaveMode(true);
            GetBacklight()->SetBrightness(10);""", """            GetDisplay()->SetPowerSaveMode(true);
            GetBacklight()->SetBrightness(0);  // tela apaga""")
trocar(placa, """    void OnKnobRotate(bool clockwise) {""", """    void OnKnobRotate(bool clockwise) {
        power_save_timer_->WakeUp();  // girar a roda acorda a tela""")
# Tela apagada: o primeiro clique só acende a tela (não abre o microfone nem escolhe item na gaveta)
trocar(XZ / "main/boards/common/power_save_timer.h", "    void WakeUp();\n",
       "    void WakeUp();\n    bool Dormindo() const { return in_sleep_mode_; }  // tela apagada\n")
trocar(placa, """        iot_button_register_cb(btns, BUTTON_SINGLE_CLICK, nullptr, [](void* button_handle, void* usr_data) {
            auto self = static_cast<SensecapWatcher*>(usr_data);
            self->power_save_timer_->WakeUp();
""", """        iot_button_register_cb(btns, BUTTON_SINGLE_CLICK, nullptr, [](void* button_handle, void* usr_data) {
            auto self = static_cast<SensecapWatcher*>(usr_data);
            bool tela_apagada = self->power_save_timer_->Dormindo();
            self->power_save_timer_->WakeUp();
            if (tela_apagada) {
                return;  // o primeiro toque só acende a tela
            }
""")
# Volume na roda: o Clawd anima enquanto gira (horário diminui, anti-horário aumenta, como no original)
trocar(placa, """        GetDisplay()->ShowNotification(std::string(Lang::Strings::VOLUME) + ": "+std::to_string(codec->output_volume()));""",
       """        GetDisplay()->ShowNotification(std::string(Lang::Strings::VOLUME) + ": "+std::to_string(codec->output_volume()));
        static_cast<CustomLcdDisplay*>(display_)->MostrarVolume(!clockwise, codec->output_volume());""")

# 15. Palavra de ativação personalizada no build.py: --wake-word "custom:hey ollie|Ollie" --------
#     MultiNet7 em inglês (fonemas gerados no aparelho pelo flite_g2p do ESP-SR).
trocar(XZ / "scripts/build.py", """    if target not in _ESP_WAKE_WORD_TARGETS | _AFE_WAKE_WORD_TARGETS:
        raise ValueError(f"Wake-word selection is not supported for target {target}")""", """    if normalized.startswith("custom:"):
        frase, _, exibicao = wake_word.split(":", 1)[1].partition("|")
        frase = frase.strip().lower()
        exibicao = (exibicao or frase).strip()
        options.extend([
            "CONFIG_USE_CUSTOM_WAKE_WORD=y",
            f'CONFIG_CUSTOM_WAKE_WORD="{frase}"',
            f'CONFIG_CUSTOM_WAKE_WORD_DISPLAY="{exibicao}"',
            "CONFIG_CUSTOM_WAKE_WORD_THRESHOLD=20",
            "CONFIG_SR_MN_CN_NONE=y",
            "CONFIG_SR_MN_EN_MULTINET7_QUANT=y",
        ])
        return "custom", options, ["CONFIG_USE_CUSTOM_WAKE_WORD"]

    if target not in _ESP_WAKE_WORD_TARGETS | _AFE_WAKE_WORD_TARGETS:
        raise ValueError(f"Wake-word selection is not supported for target {target}")""")

# 16. Clawd anima conforme o estado: conectando, ouvindo e falando ---------------------------
#     Enquanto fala, a animação "falando" vale mais que a emoção escolhida pelo LLM.
trocar(app_cc, """            display->SetStatus(Lang::Strings::CONNECTING);
            display->SetEmotion("neutral");""", """            display->SetStatus(Lang::Strings::CONNECTING);
            display->SetEmotion("conectando");""")
trocar(app_cc, """            display->SetStatus(Lang::Strings::LISTENING);
            display->SetEmotion("neutral");""", """            display->SetStatus(Lang::Strings::LISTENING);
            display->SetEmotion("ouvindo");""")
trocar(app_cc, """            display->SetStatus(Lang::Strings::SPEAKING);

            if (listening_mode_ != kListeningModeRealtime) {""", """            display->SetStatus(Lang::Strings::SPEAKING);
            display->SetEmotion("falando");

            if (listening_mode_ != kListeningModeRealtime) {""")
trocar(app_cc, """                Schedule([display, emotion_str = std::string(emotion->valuestring)]() {
                    display->SetEmotion(emotion_str.c_str());""", """                Schedule([this, display, emotion_str = std::string(emotion->valuestring)]() {
                    if (GetDeviceState() == kDeviceStateSpeaking) return;  // mantém o Clawd falando
                    display->SetEmotion(emotion_str.c_str());""")

# 17. Palavra de ativação = "Hey <nome do agente>" escolhido em Configurações (Settings watcher/agente)
#     O nome compilado (CONFIG_CUSTOM_WAKE_WORD) vale até a primeira escolha. Lido ao iniciar.
cww = XZ / "main/audio/wake_words/custom_wake_word.cc"
trocar(cww, '#include "assets.h"\n', '#include "assets.h"\n#include "settings.h"\n\n#include <algorithm>\n#include <cctype>\n')
trocar(cww, """        ParseWakenetModelConfig();
    }
""", """        ParseWakenetModelConfig();
    }

    // Nome do agente escolhido no Watcher: troca a frase de ativação (a do pacote de assets ou a compilada)
    {
        Settings ajustes("watcher", false);
        std::string agente = ajustes.GetString("agente");
        if (!agente.empty()) {
            std::string frase = "hey " + agente;
            std::transform(frase.begin(), frase.end(), frase.begin(), [](unsigned char ch) { return std::tolower(ch); });
            if (frase == "hey clawd") {
                frase = "hey claude";  // mesma pronúncia, fonemas mais confiáveis
            }
            bool trocou = false;
            for (auto& c : commands_) {
                if (c.action == "wake") {
                    c.command = frase;
                    c.text = agente;
                    trocou = true;
                }
            }
            if (!trocou) {
                commands_.push_back({frase, agente, "wake"});
            }
            ESP_LOGI(TAG, "Palavra de ativação do agente: %s (%s)", frase.c_str(), agente.c_str());
        }
    }
""")

# 18. Tela de carregamento: "Verificando atualização" no centro (no topo ele rolava e saía do arco)
#     e sem a linha técnica (nome da placa/versão) que aparecia ao ligar
trocar(app_cc, """    display->SetChatMessage("system", SystemInfo::GetUserAgent().c_str());""",
       """    ESP_LOGI(TAG, "%s", SystemInfo::GetUserAgent().c_str());  // fica só no log""")
trocar(app_cc, """        display->SetStatus(Lang::Strings::CHECKING_NEW_VERSION);
""", """        display->SetStatus("");
        display->SetChatMessage("carregando", Lang::Strings::CHECKING_NEW_VERSION);
""")
trocar(XZ / "main/assets/locales/pt-BR/language.json", '"CHECKING_NEW_VERSION": "Verificando nova versão..."',
       '"CHECKING_NEW_VERSION": "Verificando atualização..."')

# 19. Getter da gravação local (o serviço de silêncio não encerra a escuta durante uma reunião)
trocar(app_h, """    bool IsVoiceDetected() const { return audio_service_.IsVoiceDetected(); }""",
       """    bool IsVoiceDetected() const { return audio_service_.IsVoiceDetected(); }
    bool GravandoLocal() const { return gravacao_local_; }""")

# 20. Respostas faladas liga/desliga (Configurações): desligadas, o áudio da resposta é descartado e
#     só o texto aparece na tela
trocar(app_h, """    bool GravandoLocal() const { return gravacao_local_; }""",
       """    bool GravandoLocal() const { return gravacao_local_; }
    bool VozLigada() const { return voz_ligada_; }
    void DefinirVoz(bool ligada) { voz_ligada_ = ligada; }
    bool voz_ligada_ = true;  // Configurações > Respostas faladas""")
trocar(app_cc, """        if (GetDeviceState() == kDeviceStateSpeaking) {
            audio_service_.PushPacketToDecodeQueue(std::move(packet));
        }""", """        if (GetDeviceState() == kDeviceStateSpeaking && voz_ligada_) {  // modo só texto: não toca a resposta
            audio_service_.PushPacketToDecodeQueue(std::move(packet));
        }""")

# 21. Câmera: expõe a última foto em JPEG (o app Câmera salva no microSD)
trocar(XZ / "main/boards/sensecap-watcher/sscma_camera.h",
       """    virtual void SetExplainUrl(const std::string& url, const std::string& token);""",
       """    virtual void SetExplainUrl(const std::string& url, const std::string& token);
    // Última foto em JPEG (preenchida por Capture); vazia se ainda não houve captura
    std::string UltimaFotoJpeg() const {
        return (jpeg_data_.buf != nullptr && jpeg_data_.len > 0) ? std::string((const char*)jpeg_data_.buf, jpeg_data_.len)
                                                                  : std::string();
    }""")

# 22. Clawd também sem internet. O original só aplica os assets (onde moram os GIFs do mascote) depois
#     que a rede conecta: sem Wi-Fi a tela ficava só com ícones de fonte. Agora os assets entram no boot,
#     antes da rede (o motor de voz só inicia depois, então os modelos da ativação seguem valendo), e a
#     ativação não aplica de novo se não houver pacote novo para baixar.
trocar(app_h, "    bool assets_version_checked_ = false;",
       "    bool assets_version_checked_ = false;\n    bool assets_aplicados_ = false;  // aplicados no boot (Clawd sem rede)")
trocar(app_cc, """    display->SetupUI();
""", """    display->SetupUI();
    {
        auto& assets = Assets::GetInstance();
        Settings ajustes_assets("assets", false);
        if (assets.partition_valid() && ajustes_assets.GetString("download_url").empty() && assets.Apply()) {
            assets_aplicados_ = true;
            display->SetEmotion("conectando");
        }
    }
""")
trocar(app_cc, """    std::string download_url = settings.GetString("download_url");

    if (!download_url.empty()) {""", """    std::string download_url = settings.GetString("download_url");

    if (download_url.empty() && assets_aplicados_) {
        display->SetEmotion("searching");  // já aplicados no boot; agora procura atualização (lupa)
        return;
    }

    if (!download_url.empty()) {""")
trocar(app_cc, """    display->SetChatMessage("system", "");
    display->SetEmotion("robot_2");""", """    display->SetChatMessage("system", "");
    display->SetEmotion("searching");""")
#     Rede caiu, servidor fora ou Wi-Fi por configurar: Clawd "offline" (tenta encaixar o cabo);
#     voltou: Clawd contente
trocar(app_cc, """            Alert(Lang::Strings::ERROR, last_error_message_.c_str(), "cancel",""",
       """            Alert(Lang::Strings::ERROR, last_error_message_.c_str(), "offline",""")
trocar(app_cc, """                display->ShowNotification(Lang::Strings::SCANNING_WIFI, 30000);
""", """                display->ShowNotification(Lang::Strings::SCANNING_WIFI, 30000);
                display->SetEmotion(GetDeviceState() == kDeviceStateStarting ? "conectando" : "offline");
""")
trocar(app_cc, """                display->ShowNotification(msg.c_str(), 30000);
                xEventGroupSetBits(event_group_, MAIN_EVENT_NETWORK_CONNECTED);""", """                display->ShowNotification(msg.c_str(), 30000);
                if (GetDeviceState() == kDeviceStateIdle) {
                    display->SetEmotion("happy");
                }
                xEventGroupSetBits(event_group_, MAIN_EVENT_NETWORK_CONNECTED);""")
trocar(app_cc, """            case NetworkEvent::Disconnected:
                xEventGroupSetBits(event_group_, MAIN_EVENT_NETWORK_DISCONNECTED);""", """            case NetworkEvent::Disconnected:
                display->SetEmotion("offline");
                xEventGroupSetBits(event_group_, MAIN_EVENT_NETWORK_DISCONNECTED);""")
trocar(app_cc, """        case kDeviceStateWifiConfiguring:
            audio_service_.EnableVoiceProcessing(false);""", """        case kDeviceStateWifiConfiguring:
            display->SetEmotion("offline");
            audio_service_.EnableVoiceProcessing(false);""")
#     Ícones do sistema pedidos pelo original (cloud_off, cancel, gear...) viram poses do Clawd, e um nome
#     que a coleção não tem cai no neutro em vez de um ícone de fonte
trocar(XZ / "main/display/lcd_display.cc", """    auto emoji_collection = static_cast<LvglTheme*>(current_theme_)->emoji_collection();
    auto image = emoji_collection != nullptr ? emoji_collection->GetEmojiImage(emotion) : nullptr;
    if (image == nullptr) {""", """    auto emoji_collection = static_cast<LvglTheme*>(current_theme_)->emoji_collection();
    auto image = emoji_collection != nullptr ? emoji_collection->GetEmojiImage(emotion) : nullptr;
    if (image == nullptr && emoji_collection != nullptr) {
        // Só com a coleção do Clawd: sem ela (antes de baixar os assets) o nome original cai no ícone de fonte
        static const char* const kApelidos[][2] = {
            {"cloud_off", "offline"}, {"gear", "offline"}, {"cancel", "chateado"}, {"link", "waving"},
            {"download", "skate"}, {"cloud_download", "skate"}, {"triste", "sad"}, {"pesquisando", "searching"}};
        for (auto& apelido : kApelidos) {
            if (strcmp(emotion, apelido[0]) == 0) {
                image = emoji_collection->GetEmojiImage(apelido[1]);
                break;
            }
        }
        if (image == nullptr) {
            image = emoji_collection->GetEmojiImage("neutral");
        }
    }
    if (image == nullptr) {""")

# 23. Pose escolhida pelo servidor (emoji no início da resposta ou ferramenta rodando): aparece na hora
#     enquanto ele pensa e também nos 3 primeiros segundos da fala; depois a boca volta a mexer ("falando").
#     "happy" e "neutral" são o padrão de quando não houve escolha: não seguram a fala.
trocar(app_h, "    bool voz_ligada_ = true;  // Configurações > Respostas faladas",
       """    bool voz_ligada_ = true;  // Configurações > Respostas faladas
    std::string emocao_resposta_;   // última pose escolhida pelo servidor neste turno
    bool emocao_na_fala_ = false;   // a pose está na tela no começo da fala
    static bool EmocaoExpressiva(const std::string& e) { return !e.empty() && e != "happy" && e != "neutral"; }
    void MostrarEmocaoResposta(const std::string& emocao);""")
trocar(app_cc, """                Schedule([this, display, emotion_str = std::string(emotion->valuestring)]() {
                    if (GetDeviceState() == kDeviceStateSpeaking) return;  // mantém o Clawd falando
                    display->SetEmotion(emotion_str.c_str());""", """                Schedule([this, display, emotion_str = std::string(emotion->valuestring)]() {
                    MostrarEmocaoResposta(emotion_str);""")
trocar(app_cc, "void Application::ToggleChatState() {", """void Application::MostrarEmocaoResposta(const std::string& emocao) {
    auto display = Board::GetInstance().GetDisplay();
    if (GetDeviceState() != kDeviceStateSpeaking) {
        emocao_resposta_ = emocao;  // pensando ou rodando ferramenta: mostra já e guarda para a fala
        display->SetEmotion(emocao.c_str());
    } else if (EmocaoExpressiva(emocao) && clock_ticks_ < 2) {  // chegou junto com a fala
        emocao_resposta_ = emocao;
        emocao_na_fala_ = true;
        display->SetEmotion(emocao.c_str());
    }
}

void Application::ToggleChatState() {""")
trocar(app_cc, """            display->SetStatus(Lang::Strings::SPEAKING);
            display->SetEmotion("falando");""", """            display->SetStatus(Lang::Strings::SPEAKING);
            emocao_na_fala_ = EmocaoExpressiva(emocao_resposta_);
            display->SetEmotion(emocao_na_fala_ ? emocao_resposta_.c_str() : "falando");""")
trocar(app_cc, """            display->SetStatus(Lang::Strings::LISTENING);
            display->SetEmotion("ouvindo");""", """            display->SetStatus(Lang::Strings::LISTENING);
            display->SetEmotion("ouvindo");
            emocao_resposta_.clear();  // turno novo
            emocao_na_fala_ = false;""")
trocar(app_cc, """            clock_ticks_++;
            auto display = Board::GetInstance().GetDisplay();
            display->UpdateStatusBar();""", """            clock_ticks_++;
            auto display = Board::GetInstance().GetDisplay();
            display->UpdateStatusBar();
            if (emocao_na_fala_ && clock_ticks_ >= 3) {  // 3 s de pose; depois a boca volta a mexer
                emocao_na_fala_ = false;
                emocao_resposta_.clear();
                if (GetDeviceState() == kDeviceStateSpeaking) {
                    display->SetEmotion("falando");
                }
            }""")

# 24. Sem Wi-Fi: tela própria com o Clawd "sem_wifi", rede do Ollie e portal que diz por que não conectou --
#     Antes: o alerta do XiaoZhi punha "Modo de configuração de rede" rolando no topo, um emoji de
#     engrenagem por cima e a dica numa frase só, com o endereço quebrando de linha.
display_h = XZ / "main/display/display.h"
trocar(display_h, """    virtual void SetEmotion(const char* emotion);""", """    virtual void SetEmotion(const char* emotion);
    // Modo de configuração de Wi-Fi: a placa pode desenhar uma tela própria (retorna false para o alerta padrão)
    virtual bool MostrarSemWifi(const std::string& rede, const std::string& url) { return false; }
    virtual void EsconderSemWifi() {}""")
wifi_board = XZ / "main/boards/common/wifi_board.cc"
trocar(wifi_board, """    config.ssid_prefix = "Xiaozhi";""", """    config.ssid_prefix = "Ollie";  // rede do portal e nome no roteador: Ollie-XXXX""")
trocar(wifi_board, """        Application::GetInstance().Alert(Lang::Strings::WIFI_CONFIG_MODE, hint.c_str(), "gear", Lang::Sounds::OGG_WIFICONFIG);""",
       """        auto display = Board::GetInstance().GetDisplay();
        if (display->MostrarSemWifi(wifi_manager.GetApSsid(), wifi_manager.GetApWebUrl())) {
            Application::GetInstance().PlaySound(Lang::Sounds::OGG_WIFICONFIG);
            return;
        }
        Application::GetInstance().Alert(Lang::Strings::WIFI_CONFIG_MODE, hint.c_str(), "gear", Lang::Sounds::OGG_WIFICONFIG);""")
trocar(wifi_board, """            ESP_LOGI(TAG, "WiFi config mode exited");""", """            ESP_LOGI(TAG, "WiFi config mode exited");
            GetDisplay()->EsconderSemWifi();""")
# A seção 22 mostra "offline" ao entrar no modo de configuração e troca o emoji "gear" por "offline":
# aqui é o Clawd sem Wi-Fi (o "offline" continua para Wi-Fi caindo, servidor fora e erro de rede)
trocar(app_cc, """        case kDeviceStateWifiConfiguring:
            display->SetEmotion("offline");""", """        case kDeviceStateWifiConfiguring:
            display->SetEmotion("sem_wifi");""")
trocar(XZ / "main/display/lcd_display.cc", """{"gear", "offline"}""", """{"gear", "sem_wifi"}""")

# Portal: em vez de "Failed to connect to the Access Point", o motivo, no idioma do aparelho.
#   O Watcher (ESP32-S3) só enxerga Wi-Fi de 2,4 GHz: rede só em 5 GHz aparece como "não encontrada".
ap_h = WIFI / "include/wifi_configuration_ap.h"
ap_cc = WIFI / "wifi_configuration_ap.cc"
trocar(ap_h, """    uint8_t last_connected_channel_ = 0;""", """    uint8_t last_connected_channel_ = 0;
    int ultimo_motivo_ = 0;  // motivo da última desconexão (wifi_err_reason_t); 0 = não respondeu
    std::string MotivoFalha(const std::string& ssid);""")
trocar(ap_cc, """    } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupSetBits(self->event_group_, WIFI_FAIL_BIT);""", """    } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
        self->ultimo_motivo_ = static_cast<wifi_event_sta_disconnected_t*>(event_data)->reason;
        xEventGroupSetBits(self->event_group_, WIFI_FAIL_BIT);""")
trocar(ap_cc, """    is_connecting_ = true;
    last_connected_channel_ = 0;""", """    is_connecting_ = true;
    last_connected_channel_ = 0;
    ultimo_motivo_ = 0;""")
trocar(ap_cc, """            if (!this_->ConnectToWifi(ssid_str, password_str)) {
                cJSON_Delete(json);
                httpd_resp_send(req, "{\\"success\\":false,\\"error\\":\\"Failed to connect to the Access Point\\"}", HTTPD_RESP_USE_STRLEN);
                return ESP_OK;""", """            if (!this_->ConnectToWifi(ssid_str, password_str)) {
                cJSON_Delete(json);
                auto resposta = cJSON_CreateObject();
                cJSON_AddBoolToObject(resposta, "success", false);
                cJSON_AddStringToObject(resposta, "error", this_->MotivoFalha(ssid_str).c_str());
                char* texto = cJSON_PrintUnformatted(resposta);
                httpd_resp_send(req, texto, HTTPD_RESP_USE_STRLEN);
                cJSON_free(texto);
                cJSON_Delete(resposta);
                return ESP_OK;""")
trocar(ap_cc, """void WifiConfigurationAp::Save(const std::string &ssid, const std::string &password)""",
       """// Por que não conectou, em palavras, no idioma do portal (pt, es, zh; o resto em inglês)
std::string WifiConfigurationAp::MotivoFalha(const std::string &ssid)
{
    auto idioma = language_.substr(0, 2);
    auto tr = [&](const char* pt, const char* en, const char* zh, const char* es) -> std::string {
        return idioma == "pt" ? pt : idioma == "zh" ? zh : idioma == "es" ? es : en;
    };
    // Mesmo nome com maiúsculas diferentes na lista de redes vistas: o nome tem de ser exato
    std::string parecida;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& rec : ap_records_) {
            std::string nome = reinterpret_cast<const char*>(rec.ssid);
            if (nome != ssid && strcasecmp(nome.c_str(), ssid.c_str()) == 0) {
                parecida = nome;
            }
        }
    }
    std::string motivo;
    switch (ultimo_motivo_) {
        case WIFI_REASON_NO_AP_FOUND:
            motivo = tr("Rede não encontrada. O Watcher só enxerga Wi-Fi de 2,4 GHz: se a rede for só de 5 GHz, ele não vê. Confira também se o nome está exato.",
                        "Network not found. The Watcher only sees 2.4 GHz Wi-Fi: a 5 GHz-only network is invisible to it. Also check the exact name.",
                        "未找到网络。Watcher 只能看到 2.4 GHz Wi-Fi，仅 5 GHz 的网络它看不到。也请检查名称是否完全一致。",
                        "Red no encontrada. El Watcher solo ve Wi-Fi de 2,4 GHz: una red solo de 5 GHz no la ve. Revisa también el nombre exacto.");
            break;
        case WIFI_REASON_AUTH_FAIL:
        case WIFI_REASON_AUTH_EXPIRE:
        case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
        case WIFI_REASON_HANDSHAKE_TIMEOUT:
            motivo = tr("A rede recusou a senha. Confira maiúsculas, minúsculas e símbolos.",
                        "The network rejected the password. Check upper/lower case and symbols.",
                        "网络拒绝了密码。请检查大小写和符号。",
                        "La red rechazó la contraseña. Revisa mayúsculas, minúsculas y símbolos.");
            break;
        case WIFI_REASON_NO_AP_FOUND_W_COMPATIBLE_SECURITY:
        case WIFI_REASON_NO_AP_FOUND_IN_AUTHMODE_THRESHOLD:
            motivo = tr("A rede usa uma segurança que o Watcher não aceita (por exemplo, Wi-Fi corporativo com usuário e senha).",
                        "The network uses security the Watcher does not support (for example, enterprise Wi-Fi with username and password).",
                        "该网络使用 Watcher 不支持的安全方式（例如需要用户名和密码的企业 Wi-Fi）。",
                        "La red usa una seguridad que el Watcher no admite (por ejemplo, Wi-Fi corporativo con usuario y contraseña).");
            break;
        case WIFI_REASON_NO_AP_FOUND_IN_RSSI_THRESHOLD:
            motivo = tr("O sinal da rede está fraco demais aqui.", "The network signal is too weak here.",
                        "这里的网络信号太弱。", "La señal de la red es demasiado débil aquí.");
            break;
        case 0:
            motivo = tr("A rede não respondeu a tempo. Tente de novo mais perto do roteador.",
                        "The network did not answer in time. Try again closer to the router.",
                        "网络没有及时响应。请靠近路由器再试。",
                        "La red no respondió a tiempo. Inténtalo de nuevo más cerca del router.");
            break;
        default:
            motivo = tr("Não conectou", "Could not connect", "无法连接", "No se conectó") + " (" +
                     std::to_string(ultimo_motivo_) + ").";
            break;
    }
    if (!parecida.empty()) {
        motivo += tr(" Achei a rede \\"", " Found the network \\"", " 找到了网络 \\"", " Encontré la red \\"") + parecida +
                  tr("\\": o nome precisa ser igual, com as mesmas maiúsculas.", "\\": the name must match exactly, including case.",
                     "\\"：名称必须完全一致，包括大小写。", "\\": el nombre debe ser igual, con las mismas mayúsculas.");
    }
    return motivo;
}

void WifiConfigurationAp::Save(const std::string &ssid, const std::string &password)""")

# 25. Atualização pela internet (sem cabo) -----------------------------------------------------------
#     O OTA do XiaoZhi troca só o programa (servidor/xiaozhi-server/data/bin/sensecap-watcher_<versão>.bin).
#     Os desenhos do Clawd ficam na partição de assets: se falta a pose mais nova (OLLIE_POSE_ASSETS),
#     o aparelho baixa a partição do mesmo servidor (data/bin/OLLIE_ARQUIVO_ASSETS), pelo download de
#     assets do próprio XiaoZhi. Ao mudar os GIFs, troque a pose e o nome do arquivo aqui.
OLLIE_VERSAO_APP = "2.5.4"
OLLIE_POSE_ASSETS = "sem_wifi"
OLLIE_ARQUIVO_ASSETS = "ollie-assets_2.bin"
trocar(XZ / "CMakeLists.txt", 'set(PROJECT_VER "2.5.0")', f'set(PROJECT_VER "{OLLIE_VERSAO_APP}")')
trocar(app_cc, '#include "websocket_protocol.h"\n', '#include "websocket_protocol.h"\n#include "lvgl_theme.h"\n')
trocar(app_cc, """    std::string download_url = settings.GetString("download_url");
""", f"""    std::string download_url = settings.GetString("download_url");
    // Ollie: desenhos defasados (atualização só do programa pela internet) ou não aplicados: baixa a
    // partição de assets do servidor do OTA. Se os desenhos antigos já foram aplicados neste boot (seção
    // 22), baixar agora desmapearia a partição com o GIF e a fonte ainda lendo dela (LoadProhibited):
    // grava o pedido e reinicia; no boot seguinte nada é aplicado antes do download. A marca
    // "ollie_tentou" evita reiniciar de novo se o arquivo faltar no servidor.
    if (download_url.empty()) {{
        auto tema = static_cast<LvglTheme*>(display->GetTheme());
        auto colecao = tema != nullptr ? tema->emoji_collection() : nullptr;
        if (!assets_aplicados_ || colecao == nullptr || colecao->GetEmojiImage("{OLLIE_POSE_ASSETS}") == nullptr) {{
            std::string url = std::string(CONFIG_OTA_URL) + "download/{OLLIE_ARQUIVO_ASSETS}";
            if (!assets_aplicados_) {{
                download_url = url;  // nada mapeado: dá para baixar já
                ESP_LOGW(TAG, "Assets não aplicados: baixando %s", url.c_str());
            }} else if (settings.GetString("ollie_tentou") != "{OLLIE_ARQUIVO_ASSETS}") {{
                {{
                    Settings pedido("assets", true);  // o destrutor grava na NVS antes de reiniciar
                    pedido.SetString("ollie_tentou", "{OLLIE_ARQUIVO_ASSETS}");
                    pedido.SetString("download_url", url);
                }}
                ESP_LOGW(TAG, "Assets sem a pose {OLLIE_POSE_ASSETS}: reiniciando para baixar %s", url.c_str());
                // Programa recém-chegado pelo OTA ainda "a verificar": reiniciar sem marcá-lo válido faz o
                // bootloader voltar à versão anterior, que baixava os assets e, se falhasse, ficava sem Clawd
                ota_->MarkCurrentVersionValid();
                Reboot();
                return;
            }}
        }}
    }}
""")
#     Partição de assets apagada (download interrompido: o cabeçalho só é gravado no fim) não é "válida",
#     e o original desistia aí: o Clawd sumia de vez. Com a partição presente, baixa de novo a cada boot
#     até dar certo (sem reiniciar: nada está mapeado).
trocar(XZ / "main/assets.h", """    inline bool partition_valid() const { return partition_valid_; }""",
       """    inline bool partition_valid() const { return partition_valid_; }
    inline bool partition_found() const { return partition_ != nullptr; }  // existe, mesmo apagada""")
trocar(app_cc, """    if (!assets.partition_valid()) {
        ESP_LOGW(TAG, "Assets partition is disabled for board %s", BOARD_NAME);""",
       """    if (!assets.partition_found()) {
        ESP_LOGW(TAG, "Assets partition is disabled for board %s", BOARD_NAME);""")

# 26. Tela de atualização do sistema (programa ou desenhos baixando): o spinner das Configurações
#     (placa/spinner_watcher.h) no lugar do Clawd, com o progresso ("37% 120KB/s") logo abaixo.
trocar(display_h, """    virtual void EsconderSemWifi() {}""", """    virtual void EsconderSemWifi() {}
    // Atualização do sistema em andamento: a placa pode mostrar um spinner
    virtual void MostrarAtualizando(bool ativo) {}""")
trocar(placa, """#include "tela_sem_wifi.h"     // modo de configuração de Wi-Fi\n""",
       """#include "tela_sem_wifi.h"     // modo de configuração de Wi-Fi\n#include "spinner_watcher.h"   // spinner da tela de atualização\n""")
trocar(placa, """        // Modo de configuração de Wi-Fi: tela própria (placa/tela_sem_wifi.h) no lugar do alerta do XiaoZhi""",
       """        // Atualização do sistema: spinner no lugar do Clawd, no centro da tela; com 100 px ele termina onde
        // começa a caixa do progresso (layout de conversa, placa/layout_mascote.h)
        lv_obj_t* spinner_atualizacao_ = nullptr;

        virtual void MostrarAtualizando(bool ativo) override {
            DisplayLockGuard lock(this);
            if (emoji_box_ == nullptr || ativo == (spinner_atualizacao_ != nullptr)) {
                return;
            }
            if (ativo) {
                spinner_atualizacao_ = SpinnerWatcher(lv_obj_get_parent(emoji_box_), 100, 12, 0x2A2A2A);
                lv_obj_align(spinner_atualizacao_, LV_ALIGN_CENTER, 0, 0);
                lv_obj_add_flag(emoji_box_, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_delete(spinner_atualizacao_);
                spinner_atualizacao_ = nullptr;
                lv_obj_remove_flag(emoji_box_, LV_OBJ_FLAG_HIDDEN);
            }
        }

        // Modo de configuração de Wi-Fi: tela própria (placa/tela_sem_wifi.h) no lugar do alerta do XiaoZhi""")
trocar(app_cc, """    led->OnStateChanged();

    switch (new_state) {""", """    led->OnStateChanged();
    display->MostrarAtualizando(new_state == kDeviceStateUpgrading);

    switch (new_state) {""")
#     Falhou: o estado pode continuar "atualizando"; o spinner sai para o alerta aparecer
trocar(app_cc, """        board.SetPowerSaveLevel(PowerSaveLevel::LOW_POWER);  // Restore power save level
        Alert(Lang::Strings::ERROR, Lang::Strings::UPGRADE_FAILED, "cancel",""",
       """        board.SetPowerSaveLevel(PowerSaveLevel::LOW_POWER);  // Restore power save level
        display->MostrarAtualizando(false);
        Alert(Lang::Strings::ERROR, Lang::Strings::UPGRADE_FAILED, "cancel",""")
trocar(app_cc, """        if (!success) {
            Alert(Lang::Strings::ERROR, Lang::Strings::DOWNLOAD_ASSETS_FAILED, "cancel",""",
       """        if (!success) {
            display->MostrarAtualizando(false);
            Alert(Lang::Strings::ERROR, Lang::Strings::DOWNLOAD_ASSETS_FAILED, "cancel",""")

if problemas:
    print("Problemas:\n  " + "\n  ".join(problemas))
    sys.exit(1)
print("Patches aplicados.")

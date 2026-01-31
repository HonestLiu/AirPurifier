#include "gui.h"
#include <u8g2.h>
#include <stdio.h>

/**
 * @brief  根据配置渲染屏幕内容
 * @param  u8g2: 指向 u8g2 结构体的指针
 * @param  cfg: 指向 UI 配置结构体的指针
 * @retval None
 */
void gui_render_screen(u8g2_t *u8g2, const ui_config_t *cfg)  {
    char buf[16];
    u8g2_ClearBuffer(u8g2);
    u8g2_SetBitmapMode(u8g2, 1);
    u8g2_SetFontMode(u8g2, 1);

    // WIFI图标
    if (cfg->show_wifi) {
        u8g2_DrawXBM(u8g2, 3, -1, 16, 16, wifi_bits);
    }

    // 湿度显示逻辑
    if (cfg->show_humidity) {
        u8g2_DrawXBM(u8g2, -2, 44, 16, 16, temp_bits);
        u8g2_SetFont(u8g2, u8g2_font_6x12_tr);
        snprintf(buf, sizeof(buf), "%d%%", cfg->humidity);
        u8g2_DrawStr(u8g2, 16, 57, buf);
    }

    // 温度显示逻辑
    if (cfg->show_temp) {
        u8g2_DrawXBM(u8g2, 43, 45, 16, 16, hum_bits);
        u8g2_SetFont(u8g2, u8g2_font_6x12_tr);
        snprintf(buf, sizeof(buf), "%d℃", cfg->temp);
        u8g2_DrawUTF8(u8g2, 61, 57, buf);
    }

    // 警告图标显示逻辑
    if (cfg->show_warning) {
        u8g2_DrawXBM(u8g2, 67, 0, 15, 16, warning_bits);
    }

    // 风扇显示逻辑
    if (cfg->show_fan) {
        u8g2_DrawXBM(u8g2, 24, 0, 17, 16, fan_bits);
    }

    // 甲醛显示逻辑
    if (cfg->show_hcho) {
        u8g2_DrawXBM(u8g2, 89, 44, 16, 16, hcho_bits);

        u8g2_SetFont(u8g2, u8g2_font_6x12_tr);
        u8g2_DrawUTF8(u8g2, 107, 57, "1µg");
    }

    // 自动模式图标
    if (cfg->auto_mode) {
        u8g2_DrawXBM(u8g2, 46, 0, 16, 16, auto_mode_bits);
    }


    // --- 3. 核心数据：PM2.5 (带位置自适应) ---
    u8g2_SetFont(u8g2, u8g2_font_t0_13b_tr);
    u8g2_DrawStr(u8g2, 6, 30, "P M");
    u8g2_DrawStr(u8g2, 6, 40, "2.5");

    u8g2_SetFont(u8g2, u8g2_font_profont29_tr);
    uint32_t x10 = cfg->pm25_raw * 4;
    snprintf(buf, sizeof(buf), "%u.%u", x10 / 10, x10 % 10);
    // 如果数字很大，稍微左移位置以防重叠
    int x_pos = (x10 >= 1000) ? 32 : 37;
    u8g2_DrawStr(u8g2, x_pos, 40, buf);


    u8g2_SendBuffer(u8g2);
}
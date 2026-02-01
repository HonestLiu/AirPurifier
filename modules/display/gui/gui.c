#include "gui.h"
#include <u8g2.h>
#include <stdio.h>

/* 定义 GUI 消息队列 */
K_MSGQ_DEFINE(gui_msgq, 
              sizeof(gui_msg_t), 
              20,   // 队列深度
              4);   // 对齐字节数

// --- 辅助函数实现 ---
void gui_set_pm25(uint16_t val) {
    gui_msg_t msg = {.type = GUI_EVT_PM25, .data.u16_val = val};
    k_msgq_put(&gui_msgq, &msg, K_NO_WAIT);
}

void gui_set_temp_hum(int16_t temp, uint16_t hum) {
    gui_msg_t msg = {
        .type = GUI_EVT_TEMP_HUM, 
        .data.th = {.temp = temp, .hum = hum}
    };
    k_msgq_put(&gui_msgq, &msg, K_NO_WAIT);
}

void gui_set_env(uint16_t tvoc, uint16_t hcho, uint16_t eco2) {
    gui_msg_t msg = {
        .type = GUI_EVT_ENV, 
        .data.env = {.tvoc = tvoc, .hcho = hcho, .eco2 = eco2}
    };
    k_msgq_put(&gui_msgq, &msg, K_NO_WAIT);
}

void gui_set_wifi(bool active) {
    gui_msg_t msg = {.type = GUI_EVT_WIFI, .data.b_val = active};
    k_msgq_put(&gui_msgq, &msg, K_NO_WAIT);
}

void gui_set_fan(bool active) {
    gui_msg_t msg = {.type = GUI_EVT_FAN, .data.b_val = active};
    k_msgq_put(&gui_msgq, &msg, K_NO_WAIT);
}

void gui_set_warning(bool active) {
    gui_msg_t msg = {.type = GUI_EVT_WARNING, .data.b_val = active};
    k_msgq_put(&gui_msgq, &msg, K_NO_WAIT);
}

void gui_set_auto_mode(bool active) {
    gui_msg_t msg = {.type = GUI_EVT_AUTO_MODE, .data.b_val = active};
    k_msgq_put(&gui_msgq, &msg, K_NO_WAIT);
}


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

    // 甲醛显示逻辑 (HCHO)
    if (cfg->show_hcho) {
        u8g2_DrawXBM(u8g2, 89, 44, 16, 16, hcho_bits);

        u8g2_SetFont(u8g2, u8g2_font_6x12_tr);
        snprintf(buf, sizeof(buf), "%d", cfg->hcho); // 假设是 ug/m3 整数
        u8g2_DrawUTF8(u8g2, 107, 57, buf); // 之前写死 "1ug"
        // u8g2_DrawUTF8(u8g2, 107, 57, "1µg");
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
    // 这里要注意：cfg->pm25_raw 是什么单位？
    // dc01_app 里： pm25_raw_x10 = total_x10;
    // gui_render_screen 里之前是： uint32_t x10 = cfg->pm25_raw * 4; 
    // 假设 gui_set_pm25 传入的是实际 PM2.5 * 10 
    
    // 如果 cfg->pm25_raw 是放大10倍的值
    snprintf(buf, sizeof(buf), "%u.%u", cfg->pm25_raw / 10, cfg->pm25_raw % 10);
    
    // 如果数字很大，稍微左移位置以防重叠
    int x_pos = (cfg->pm25_raw >= 1000) ? 32 : 37;
    u8g2_DrawStr(u8g2, x_pos, 40, buf);


    u8g2_SendBuffer(u8g2);
}
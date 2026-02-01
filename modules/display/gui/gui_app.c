#include "gui_app.h"
#include <zephyr/kernel.h>
#include <stdlib.h>

int u8g2_init(u8g2_t *u8g2)
{
    /* 初始化 u8g2：
       - 使用 SSD1306 128x64 非标驱动 (noname)
       - 全缓冲模式 (f)
       - 硬件 I2C 接口
    */
    u8g2_Setup_ssd1306_i2c_128x64_noname_f(u8g2, U8G2_R0, u8x8_byte_zephyr_i2c, u8x8_gpio_and_delay_zephyr);

    /* 初始化显示器 */
    u8g2_InitDisplay(u8g2);
    /* 唤醒显示器（取消休眠） */
    u8g2_SetPowerSave(u8g2, 0);

    return 0;
}

/* GUI 渲染线程 */
void gui_thread_func(void *a, void *b, void *c)
{
    printk("GUI thread started\n");
    u8g2_t u8g2;

    /* 初始化 u8g2 */
    u8g2_init(&u8g2);

    gui_msg_t msg;

    // 初始化配置：默认全部开启显示
    ui_config_t display_cfg = {
        .show_wifi = false,    // 默认不显示，直到连接
        .show_fan = true,      // 显示风扇图标
        .auto_mode = true,     // 显示自动模式图标
        .show_warning = false, // 默认不显示警告
        .show_humidity = true, // 显示湿度逻辑
        .show_temp = true,     // 显示温度逻辑
        .show_hcho = true,     // 显示甲醛逻辑
        .pm25_raw = 0,
        .humidity = 0,
        .temp = 0,
        .hcho = 0,
    };

    // 先渲染一帧初始界面
    gui_render_screen(&u8g2, &display_cfg);

    while (1)
    {
        // 阻塞等待消息
        if (k_msgq_get(&gui_msgq, &msg, K_FOREVER) == 0)
        {
            // 根据消息类型更新本地状态
            switch (msg.type) {
                case GUI_EVT_PM25:
                    display_cfg.pm25_raw = msg.data.u16_val;
                    // 自定义警告逻辑：PM2.5 > 150 显示警告
                    // 如果需要业务逻辑控制，也可以由外部发 GUI_EVT_WARNING 消息
                    // display_cfg.show_warning = (display_cfg.pm25_raw > 1500); // 假设是x10
                    break;
                case GUI_EVT_TEMP_HUM:
                    display_cfg.temp = msg.data.th.temp;
                    display_cfg.humidity = msg.data.th.hum;
                    break;
                case GUI_EVT_ENV:
                    display_cfg.tvoc = msg.data.env.tvoc;
                    display_cfg.hcho = msg.data.env.hcho;
                    display_cfg.co2  = msg.data.env.eco2;
                    break;
                case GUI_EVT_WIFI:
                    display_cfg.show_wifi = msg.data.b_val;
                    break;
                case GUI_EVT_FAN:
                    display_cfg.show_fan = msg.data.b_val;
                    break;
                case GUI_EVT_WARNING:
                    display_cfg.show_warning = msg.data.b_val;
                    break;
                case GUI_EVT_AUTO_MODE:
                    display_cfg.auto_mode = msg.data.b_val;
                    break;
                default:
                    break;
            }

            // 更新屏幕
            gui_render_screen(&u8g2, &display_cfg);
        }
    }
}

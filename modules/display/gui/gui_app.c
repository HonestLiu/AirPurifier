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

K_MSGQ_DEFINE(d01_msgq, sizeof(uint16_t), 10, 4);

/* 模拟数据生成线程 (后续接入传感器只需往队列发数据即可) */
void mock_sensor_task(void)
{
    uint16_t mock_val;
    while (1)
    {
        mock_val = 40 + (rand() % 200);
        k_msgq_put(&d01_msgq, &mock_val, K_NO_WAIT);
        k_msleep(1000);
    }
}
K_THREAD_DEFINE(mock_tid, 1024, mock_sensor_task, NULL, NULL, NULL, 7, 0, 0);

/* GUI 渲染线程 */
void gui_thread_func(void *a, void *b, void *c)
{
    printk("GUI thread started\n");
    u8g2_t u8g2;

    /* 初始化 u8g2：
       - 使用 SSD1306 128x64 非标驱动 (noname)
       - 全缓冲模式 (f)
       - 硬件 I2C 接口
    */
    u8g2_init(&u8g2);

    uint16_t raw_data;
    // 初始化配置：默认全部开启显示
    ui_config_t display_cfg = {
        .show_wifi = true,     // 显示wifi图标
        .show_fan = true,      // 显示风扇图标
        .auto_mode = true,     // 显示自动模式图标
        .show_warning = true,  // 显示警告图标
        .show_humidity = true, // 显示湿度逻辑
        .show_temp = true,     // 显示温度逻辑
        .show_hcho = true,     // 显示甲醛逻辑
        .humidity = 40,        // 湿度百分比
        .temp = 24,            // 温度摄氏度
        .hcho = 1,             // 甲醛数值
    };

    while (1)
    {
        // 等待数据包（模拟或真实串口 ISR 均可）
        if (k_msgq_get(&d01_msgq, &raw_data, K_FOREVER) == 0)
        {

            // 1. 动态更新 PM2.5 数据
            display_cfg.pm25_raw = raw_data;

            // 2. 模拟动态逻辑：高浓度时隐藏 WiFi 显示警告
            display_cfg.show_warning = (raw_data > 150);

            // 3. 模拟动态逻辑：随机变动温湿度
            display_cfg.humidity = 40 + (rand() % 5);

            // 4. 调用 GUI 封装层进行渲染
            gui_render_screen(&u8g2, &display_cfg);
        }
    }
}

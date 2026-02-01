#include "sensor_center.h"
#include "gui.h"  // 引入 GUI 接口

// 消息队列：用于传递传感器数据到中心处理模块
K_MSGQ_DEFINE(sensor_hub_queue, 
              sizeof(struct sensor_event), 
              20,   // 队列深度
              4);   // 对齐字节数（通常 4 或 8）



/**
 * @brief 发送传感器事件到中心消息队列
 * */              
int sensor_hub_send(const struct sensor_event *ev, k_timeout_t timeout)
{
    return k_msgq_put(&sensor_hub_queue, ev, timeout);
}

/**
 * @brief 处理 DC01 传感器数据
 * @param pm25_raw_x10 放大10倍的 PM2.5 原始值
 */
void dc01_data_process(uint32_t pm25_raw_x10) {
    uint32_t integer_part = pm25_raw_x10 / 10; // 整数部分
    uint32_t decimal_part = pm25_raw_x10 % 10; // 小数部分（十分位）
    printk("[DATA] Raw: %u | Concentration: %u.%u ug/m3\n",pm25_raw_x10 / 4, integer_part, decimal_part);
    
    // 更新 GUI PM2.5 显示
    gui_set_pm25((uint16_t)pm25_raw_x10); 
}

void tovc_data_process(uint16_t tvoc, uint16_t hcho, uint16_t eco2) {
    printk("[TVOC] %u ug/m3 | [HCHO] %u ug/m3 | [eCO2] %u ppm\n", 
            tvoc, hcho, eco2);
    
    // 更新 GUI 环境数据
    gui_set_env(tvoc, hcho, eco2);
}

void aht10_data_process(int32_t temp_x1000, int32_t hum_x1000) {

    // 处理负数（Zephyr 的 sensor_value 支持负值）
    printk("[AHT10] Temperature: ");
    if (temp_x1000 < 0) {
        printk("-%u.%03u", (-temp_x1000)/1000, (-temp_x1000)%1000);
    } else {
        printk("%u.%03u", temp_x1000/1000, temp_x1000%1000);
    }

    // 湿度不存在负数，正常处理
    printk(" | Humidity: %u.%03u %%\n", hum_x1000/1000, hum_x1000%1000);
    
    // 更新 GUI 温湿度
    // GUI 接口期望 整数温度 和 整数湿度 (或放大倍数，需统一)
    // 根据 gui.c 里的 snprintf(buf, sizeof(buf), "%d%%", cfg->humidity/10); 
    // 假设 gui_set_temp_hum 接收的是 x1000 ? 不，gui.c 没除以1000。
    
    // 让我们看一眼 gui.c:
    // snprintf(buf, sizeof(buf), "%d%%", cfg->humidity/10); -> 若传入75690, /10 = 7569%。这不对。
    // aht10 传的是 temp.val1 * 1000 + temp.val2 / 1000。也就是 放大1000倍。 25度 -> 25000。
    // 如果 gui 想要正常显示，需要 25。
    // 注意: 我刚才修改 gui.c 时，直接 snprintf(buf, sizeof(buf), "%d%%", cfg->humidity/10); 这行代码是我保留的旧逻辑还是写的新的？
    // 我看 gui.c 原文是: snprintf(buf, sizeof(buf), "%d%%", cfg->humidity); 
    // Wait, let's fix the logic in gui.c too or fix input here.
    
    // 为了简单，我们在这里转换为整数传给 GUI.
    int16_t temp_int = temp_x1000 / 1000;
    uint16_t hum_int = hum_x1000 / 1000;
    gui_set_temp_hum(temp_int, hum_int);
}

/**
 * @brief 传感器中心处理线程入口
 * */
void sensor_hub_thread(void *p1, void *p2, void *p3)
{
    struct sensor_event ev;
    while (1) {
        if (k_msgq_get(&sensor_hub_queue, &ev, K_FOREVER) == 0) {
            // 分发逻辑...
            switch (ev.type) {
                case SENSOR_DC01_PM25:
                    dc01_data_process(ev.data.dc01.pm25_raw_x10); // 传入原始值(x10)
                    break;
                case SENSOR_TOVC_301:
                    tovc_data_process(ev.data.tovc.tvoc, ev.data.tovc.hcho, ev.data.tovc.eco2);
                    break;
                case SENSOR_AHT10:
                    aht10_data_process(ev.data.aht10.temp_x1000, ev.data.aht10.hum_x1000);
                    break;
                default:
                    printk("Unknown sensor event type: %d\n", ev.type);
                    break;
            }
        }
    }
}
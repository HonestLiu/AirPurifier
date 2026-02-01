#include "sensor_center.h"


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
    // 这里可以添加更多处理逻辑，比如数据存储、上报等
}

void tovc_data_process(uint16_t tvoc, uint16_t hcho, uint16_t eco2) {
    printk("[TVOC] %u ug/m3 | [HCHO] %u ug/m3 | [eCO2] %u ppm\n", 
            tvoc, hcho, eco2);
    // 这里可以添加更多处理逻辑，比如数据存储、上报等
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
    // 这里可以添加更多处理逻辑，比如数据存储、上报等
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
                    dc01_data_process(ev.data.dc01.pm25_raw_x10 / 10); // 传入原始值除以10后的结果
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
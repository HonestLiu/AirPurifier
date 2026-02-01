#include "sensor_center.h"
#include <zephyr/sys/printk.h>
#include "control_center.h"

// 消息队列定义
K_MSGQ_DEFINE(sensor_hub_queue, sizeof(struct sensor_event), 20, 4);

/**
 * @brief 发送传感器事件
 */
int sensor_hub_send(const struct sensor_event *ev, k_timeout_t timeout)
{
    return k_msgq_put(&sensor_hub_queue, ev, timeout);
}

// --- 数据处理函数 ---

static void dc01_data_process(uint32_t pm25_raw_x10) {
    uint32_t pm25_val = pm25_raw_x10 / 10;
    // printk("[Sensor] PM2.5: %u ug/m3\n", pm25_val);
    
    // 上报到控制中心
    control_report_pm25(pm25_val);
}

static void tovc_data_process(uint16_t tvoc, uint16_t hcho, uint16_t eco2) {
    // printk("[Sensor] TVOC: %u, HCHO: %u, eCO2: %u\n", tvoc, hcho, eco2);
    
    // 上报到控制中心
    control_report_env(tvoc, hcho, eco2);
}

static void aht10_data_process(int32_t temp_x1000, int32_t hum_x1000) {
    float t = (float)temp_x1000 / 1000.0f;
    float h = (float)hum_x1000 / 1000.0f;
    // printk("[Sensor] Temp: %.1f, Hum: %.1f\n", t, h);
    
    // 上报到控制中心
    control_report_temp_hum(t, h);
}


/**
 * @brief 传感器中心线程
 */
void sensor_hub_thread(void *p1, void *p2, void *p3)
{
    struct sensor_event ev;
    
    while (1) {
        if (k_msgq_get(&sensor_hub_queue, &ev, K_FOREVER) == 0) {
            switch (ev.type) {
                case SENSOR_DC01_PM25:
                    dc01_data_process(ev.data.dc01.pm25_raw_x10);
                    break;
                case SENSOR_TOVC_301:
                    tovc_data_process(ev.data.tovc.tvoc, 
                                      ev.data.tovc.hcho, 
                                      ev.data.tovc.eco2);
                    break;
                case SENSOR_AHT10:
                    aht10_data_process(ev.data.aht10.temp_x1000, 
                                       ev.data.aht10.hum_x1000);
                    break;
                default:
                    printk("Unknown sensor event: %d\n", ev.type);
                    break;
            }
        }
    }
}

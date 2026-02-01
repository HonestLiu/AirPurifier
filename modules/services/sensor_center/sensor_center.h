#ifndef __SENSOR_CENTER_H
#define __SENSOR_CENTER_H

#include <zephyr/types.h>
#include <zephyr/kernel.h>

struct sensor_event {
    enum { 
        SENSOR_DC01_PM25,
        SENSOR_TOVC_301,
        SENSOR_AHT10,
    } type;
    int64_t timestamp;
    union {
        // DC01 PM2.5 传感器数据
        struct {
            uint32_t pm25_raw_x10; // PM2.5 原始值，放大10倍以支持小数
        } dc01;

        // TOVC-301 传感器数据
        struct {
            uint16_t tvoc; // ug/m3
            uint16_t hcho; // ug/m3
            uint16_t eco2; // ppm
        } tovc;
        
        // AHT10 传感器数据
        struct {
            int32_t temp_x1000; // 温度值，放大1000倍以支持小数
            int32_t hum_x1000;  // 湿度值
        } aht10;
    } data;
};


// 消息队列：用于传递传感器数据到中心处理模块
extern struct k_msgq sensor_hub_queue;

int sensor_hub_send(const struct sensor_event *ev, k_timeout_t timeout);
void sensor_hub_thread(void *p1, void *p2, void *p3);

#endif // !__SENSOR_CENTER_H
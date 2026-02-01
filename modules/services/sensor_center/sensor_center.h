#ifndef __SENSOR_CENTER_H
#define __SENSOR_CENTER_H

#include <zephyr/types.h>
#include <zephyr/kernel.h>

struct sensor_event {
    enum { 
        SENSOR_DC01_PM25,
    } type;
    int64_t timestamp;
    union {
        uint32_t pm25_raw_x10; // PM2.5 原始值，放大10倍以支持小数
    } data;
};


// 消息队列：用于传递传感器数据到中心处理模块
extern struct k_msgq sensor_hub_queue;

int sensor_hub_send(const struct sensor_event *ev, k_timeout_t timeout);
void sensor_hub_thread(void *p1, void *p2, void *p3);


#endif // !__SENSOR_CENTER_H
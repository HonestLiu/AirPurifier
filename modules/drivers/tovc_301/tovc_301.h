#ifndef __TOVC_301_H
#define __TOVC_301_H

#include <zephyr/types.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <string.h>
#include <zephyr/sys/printk.h>

#define SENSOR_CENTER_ENABLE 1 // 使用传感器中心模块(否则直接打印数据)

#if SENSOR_CENTER_ENABLE 
#include "sensor_center.h"
#endif

/* 定义传感器数据结构 */
typedef struct {
    uint16_t tvoc; // ug/m3
    uint16_t hcho; // ug/m3 (甲醛)
    uint16_t eco2; // ppm
} tvoc_data_t;

int tovc_sensor_app_start(void);
#endif // !__TOVC_301_H
/* drivers/sensor/aht10/aht10.h */
// Copyright (c) 2026 刘君
// SPDX-License-Identifier: Apache-2.0
// AHT10的寄存器地址是通过设备树获取的，代码中没有硬编码地址

#ifndef ZEPHYR_DRIVERS_SENSOR_AHT10_AHT10_H_
#define ZEPHYR_DRIVERS_SENSOR_AHT10_AHT10_H_

#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>

/* AHT10 命令定义 */
#define AHT10_CMD_INIT     0xE1  /* 初始化命令 */
#define AHT10_CMD_MEASURE  0xAC  /* 触发测量命令 */
#define AHT10_CMD_RESET    0xBA  /* 软复位命令 */

/* 数据掩码 */
#define AHT10_HUMIDITY_MASK     0xFFFFF000U  /* 高 20 位：湿度 */
#define AHT10_TEMPERATURE_MASK  0x000FFFFCU  /* 中 20 位：温度 */
#define AHT10_STATUS_BUSY       BIT(7)       /* 忙标志 */

/* 缩放因子（AHT10 输出为 20 位，满量程对应 100% RH / 200°C） */
#define AHT10_HUMIDITY_SCALE    1048576.0f   /* 2^20 */
#define AHT10_TEMPERATURE_SCALE 1048576.0f   /* 2^20 */

/* 配置结构体（Devicetree 提供） */
struct aht10_config {
    struct i2c_dt_spec i2c;  /* I²C 总线 + 地址 */
};

/* 运行时数据结构 */
struct aht10_data {
    uint32_t raw_humidity;   /* 原始湿度值（20 位） */
    uint32_t raw_temperature; /* 原始温度值（20 位） */
};

#endif /* ZEPHYR_DRIVERS_SENSOR_AHT10_AHT10_H_ */
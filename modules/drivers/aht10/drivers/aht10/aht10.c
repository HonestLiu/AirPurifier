/* drivers/sensor/aht10/aht10.c */
// Copyright (c) 2026 刘君
// SPDX-License-Identifier: Apache-2.0
// AHT10的寄存器地址是通过设备树获取的，代码中没有硬编码地址

#define DT_DRV_COMPAT aosong_aht10

#include <zephyr/kernel.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/i2c.h>

#include "aht10.h"

LOG_MODULE_REGISTER(AHT10, CONFIG_SENSOR_LOG_LEVEL);

/* 状态位定义 */
#define AHT10_STATUS_BUSY      BIT(7)
#define AHT10_STATUS_CALIBRATED BIT(3)

/* 延时参数 */
#define AHT10_RESET_DELAY_MS   20
#define AHT10_INIT_DELAY_MS    10
#define AHT10_MEASURE_DELAY_MS 80  /* 手册建议测量至少等待 75ms */

/* 初始化 AHT10 */
static int aht10_send_init_cmd(const struct device *dev)
{
    const struct aht10_config *cfg = dev->config;
    /* 手册要求初始化发送 0xE1 后跟 0x08, 0x00 */
    uint8_t init_cmd[] = { AHT10_CMD_INIT, 0x08, 0x00 };

    return i2c_write_dt(&cfg->i2c, init_cmd, sizeof(init_cmd));
}

/* 触发一次温湿度测量 */
static int aht10_trigger_measurement(const struct device *dev)
{
    const struct aht10_config *cfg = dev->config;
    /* 触发测量命令：0xAC 后跟 0x33, 0x00 */
    uint8_t measure_cmd[] = { AHT10_CMD_MEASURE, 0x33, 0x00 };

    return i2c_write_dt(&cfg->i2c, measure_cmd, sizeof(measure_cmd));
}

/* 驱动初始化 */
static int aht10_init(const struct device *dev)
{
    const struct aht10_config *cfg = dev->config;

    if (!device_is_ready(cfg->i2c.bus)) {
        LOG_ERR("I2C bus not ready");
        return -ENODEV;
    }

    /* 1. 软复位 */
    uint8_t reset_cmd = AHT10_CMD_RESET;
    (void)i2c_write_dt(&cfg->i2c, &reset_cmd, 1);
    k_msleep(AHT10_RESET_DELAY_MS);

    /* 2. 发送初始化命令 */
    if (aht10_send_init_cmd(dev) != 0) {
        LOG_ERR("Failed to send init command");
        return -EIO;
    }
    k_msleep(AHT10_INIT_DELAY_MS);

    /* 3. 检查校准标志 */
    uint8_t status;
    if (i2c_read_dt(&cfg->i2c, &status, 1) == 0) {
        if (!(status & AHT10_STATUS_CALIBRATED)) {
            LOG_WRN("AHT10 not calibrated after init");
        }
    }

    LOG_INF("AHT10 initialized");
    return 0;
}

/* 获取传感器原始数据 */
static int aht10_sample_fetch(const struct device *dev, enum sensor_channel chan)
{
    struct aht10_data *data = dev->data;
    const struct aht10_config *cfg = dev->config;
    uint8_t buf[6];
    int ret;

    if (chan != SENSOR_CHAN_ALL && 
        chan != SENSOR_CHAN_AMBIENT_TEMP && 
        chan != SENSOR_CHAN_HUMIDITY) {
        return -ENOTSUP;
    }

    /* 触发测量 */
    ret = aht10_trigger_measurement(dev);
    if (ret != 0) return ret;

    /* 等待测量完成：AHT10 典型测量时间为 75ms */
    k_msleep(AHT10_MEASURE_DELAY_MS);

    /* 读取 6 字节数据 */
    ret = i2c_read_dt(&cfg->i2c, buf, 6);
    if (ret != 0) {
        LOG_ERR("Failed to read data");
        return ret;
    }

    /* 检查忙标志 */
    if (buf[0] & AHT10_STATUS_BUSY) {
        LOG_WRN("AHT10 busy");
        return -EBUSY;
    }

    /* * 数据解析：
     * Byte1: RH[19:12]
     * Byte2: RH[11:4]
     * Byte3: RH[3:0] (高4位) | T[19:16] (低4位)
     * Byte4: T[15:8]
     * Byte5: T[7:0]
     */
    data->raw_humidity = ((uint32_t)buf[1] << 12) | 
                         ((uint32_t)buf[2] << 4)  | 
                         ((uint32_t)buf[3] >> 4);

    data->raw_temperature = ((uint32_t)(buf[3] & 0x0F) << 16) | 
                            ((uint32_t)buf[4] << 8)           | 
                            (uint32_t)buf[5];

    return 0;
}

/* 将原始值转换为标准 sensor_value */
static int aht10_channel_get(const struct device *dev,
                             enum sensor_channel chan,
                             struct sensor_value *val)
{
    const struct aht10_data *data = dev->data;
    int64_t tmp;

    if (chan == SENSOR_CHAN_AMBIENT_TEMP) {
        /* T = (raw / 2^20) * 200 - 50 */
        /* 使用 int64 放大 10^6 倍进行定点运算，保持精度 */
        tmp = ((int64_t)data->raw_temperature * 200 * 1000000) >> 20;
        tmp -= (50LL * 1000000);
        val->val1 = (int32_t)(tmp / 1000000);
        val->val2 = (int32_t)(tmp % 1000000);
    } else if (chan == SENSOR_CHAN_HUMIDITY) {
        /* RH = (raw / 2^20) * 100 */
        tmp = ((int64_t)data->raw_humidity * 100 * 1000000) >> 20;
        val->val1 = (int32_t)(tmp / 1000000);
        val->val2 = (int32_t)(tmp % 1000000);
    } else {
        return -ENOTSUP;
    }

    return 0;
}

static const struct sensor_driver_api aht10_api = {
    .sample_fetch = aht10_sample_fetch,
    .channel_get = aht10_channel_get,
};

#define AHT10_DEFINE(inst) \
    static struct aht10_data aht10_data_##inst; \
    static const struct aht10_config aht10_config_##inst = { \
        .i2c = I2C_DT_SPEC_INST_GET(inst), \
    }; \
    SENSOR_DEVICE_DT_INST_DEFINE(inst, \
                                 aht10_init, \
                                 NULL, \
                                 &aht10_data_##inst, \
                                 &aht10_config_##inst, \
                                 POST_KERNEL, \
                                 CONFIG_SENSOR_INIT_PRIORITY, \
                                 &aht10_api);

DT_INST_FOREACH_STATUS_OKAY(AHT10_DEFINE)
#ifndef ZEPHYR_DRIVERS_SENSOR_DC01_H_
#define ZEPHYR_DRIVERS_SENSOR_DC01_H_

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/ring_buffer.h>

#define D01_FILTER_SIZE 10

struct d01_config {
    const struct device *uart_dev;
};

struct d01_data {
    uint16_t pm25_raw;             /* 存储 sample_fetch 取出的最新值 */
    
    /* ISR 解析状态机相关变量 */
    uint8_t  rx_buf[4];            /* 临时解析缓冲 */
    uint8_t  rx_pos;               /* 当前解析位置 */
    
    /* 消息队列：用于 ISR 传数据给 Thread */
    struct k_msgq rx_msgq;
    char rx_msgq_buffer[10 * sizeof(uint16_t)]; /* 深度为 10 的缓冲池 */

    /* Debug Counters */
    uint32_t isr_cnt;
    uint32_t rx_cnt;
};

#endif
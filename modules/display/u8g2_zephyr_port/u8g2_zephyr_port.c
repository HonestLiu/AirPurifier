#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <u8g2.h>
#include "u8g2_zephyr_port.h"

/* Get the node from the device tree */
#define OLED_NODE DT_NODELABEL(ssd1306_node)

/* Validate that the node exists and is on an I2C bus */
#if !DT_NODE_HAS_STATUS(OLED_NODE, okay)
#error "Unsupported board: ssd1306_node is not defined or enabled in the device tree"
#endif

/* Get the I2C device which controls this OLED */
static const struct device *i2c_dev = DEVICE_DT_GET(DT_BUS(OLED_NODE));
static const uint16_t i2c_addr = DT_REG_ADDR(OLED_NODE);

/**
 * @brief  Zephyr I2C byte communication callback for u8g2
 * @param  u8x8: Pointer to the u8x8 structure
 * @param  msg: Message type
 * @param  arg_int: Integer argument
 * @param  arg_ptr: Pointer argument
 * @retval 1 on success, 0 on failure
 */
uint8_t u8x8_byte_zephyr_i2c(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr) {
    static uint8_t buffer[128];
    static uint8_t buf_idx;

    switch (msg) {
        case U8X8_MSG_BYTE_INIT: // 初始化 I2C 设备
            if (!device_is_ready(i2c_dev)) {
                return 0;
            }
            break;
        case U8X8_MSG_BYTE_START_TRANSFER:
            buf_idx = 0;
            break;
        case U8X8_MSG_BYTE_SEND: // 发送数据到缓冲区
            if (buf_idx + arg_int < sizeof(buffer)) {
                memcpy(&buffer[buf_idx], arg_ptr, arg_int);
                buf_idx += arg_int;
            }
            break;
        case U8X8_MSG_BYTE_END_TRANSFER: // 通过 I2C 发送缓冲区数据
            if (i2c_write(i2c_dev, buffer, buf_idx, i2c_addr) != 0) {
                return 0;
            }
            break;
        case U8X8_MSG_BYTE_SET_DC:
             /* For I2C, DC is usually handled by the command/data byte prefix, u8g2 handles this logic internally for I2C usually */
            break;
        default: return 0;
    }
    return 1;
}

/**
 * @brief  Zephyr GPIO and delay callback for u8g2
 * @param  u8x8: Pointer to the u8x8 structure
 * @param  msg: Message type
 * @param  arg_int: Integer argument
 * @param  arg_ptr: Pointer argument
 * @retval 1 on success, 0 on failure
 */
uint8_t u8x8_gpio_and_delay_zephyr(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr) {
    switch (msg) {
        case U8X8_MSG_GPIO_AND_DELAY_INIT:
             /* No specific GPIO init needed for I2C usually (handled by bus driver) */
            break;
        case U8X8_MSG_DELAY_MILLI: // 毫秒延时
            k_msleep(arg_int);
            break;
        case U8X8_MSG_DELAY_10MICRO: // 10 微秒延时
            k_usleep(10);
            break;
        case U8X8_MSG_DELAY_100NANO: // 100 纳秒延时
            k_busy_wait(1); /* 1us is smallest reliable wait, close enough */
            break;
    }
    return 1;
}
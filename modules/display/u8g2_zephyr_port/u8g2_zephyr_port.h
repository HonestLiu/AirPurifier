#ifndef U8G2_ZEPHYR_PORT_H
#define U8G2_ZEPHYR_PORT_H

#include <stdint.h>
#include <u8g2.h>

/* I2C, use u8x8_byte_zephyr_i2c */
uint8_t u8x8_byte_zephyr_i2c(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr);

/* GPIO and Delay, use u8x8_gpio_and_delay_zephyr */
uint8_t u8x8_gpio_and_delay_zephyr(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr);

#endif /* U8G2_ZEPHYR_PORT_H */

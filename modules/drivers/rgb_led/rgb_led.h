#ifndef __RGB_LED_H__
#define __RGB_LED_H__
#include <zephyr/types.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <zephyr/drivers/pwm.h>

// 启动 RGB LED 控制应用
int rgb_led_init(void);
// 设置 RGB LED 颜色，参数为 0-255 范围的 R,G,B 值
int rgb_led_set_color(uint8_t r, uint8_t g, uint8_t b);

#endif // !__RGB_LED_H__
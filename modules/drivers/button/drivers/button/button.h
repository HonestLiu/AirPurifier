#ifndef ZEPHYR_DRIVERS_BUTTON_H_
#define ZEPHYR_DRIVERS_BUTTON_H_

#include <zephyr/drivers/gpio.h>

// 自定义 API 接口（因未使用标准 sensor API）
struct button_api {
    int (*get)(const struct device *dev, uint8_t *state);
};

// 驱动配置结构体（由设备树填充）
struct button_config {
    struct gpio_dt_spec btn; 	// GPIO 引脚描述
    uint32_t id;	        // 实例 ID（用于调试）
};

#endif /* ZEPHYR_DRIVERS_BUTTON_H_ */

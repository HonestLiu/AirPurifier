#include "rgb_led.h"

#define RED_PWM_NODE DT_ALIAS(red_pwm_led)
#define GREEN_PWM_NODE DT_ALIAS(green_pwm_led)
#define BLUE_PWM_NODE DT_ALIAS(blue_pwm_led)

#if DT_NODE_EXISTS(RED_PWM_NODE) || DT_NODE_EXISTS(GREEN_PWM_NODE) || DT_NODE_EXISTS(BLUE_PWM_NODE)
static const struct pwm_dt_spec red_pwm = PWM_DT_SPEC_GET(RED_PWM_NODE);
static const struct pwm_dt_spec green_pwm = PWM_DT_SPEC_GET(GREEN_PWM_NODE);
static const struct pwm_dt_spec blue_pwm = PWM_DT_SPEC_GET(BLUE_PWM_NODE);
#else
#error "RGB LED PWM device not found in DTS"
#endif


/**
 * 设置RGB LED颜色
 * @param r 红色强度 (0-255)
 * @param g 绿色强度 (0-255)
 * @param b 蓝色强度 (0-255)
 * @return 0 成功, 负数表示错误
 */
int rgb_led_set_color(uint8_t r, uint8_t g, uint8_t b) {
    uint32_t pulse_red = (r * red_pwm.period) / 255;
    uint32_t pulse_green = (g * green_pwm.period) / 255;
    uint32_t pulse_blue = (b * blue_pwm.period) / 255;
    int ret;

    printk("设置RGB颜色: R=%d, G=%d, B=%d\n", r, g, b);

    ret = pwm_set_pulse_dt(&red_pwm, pulse_red);
    if (ret != 0) {
        printk("Error %d: red write failed\n", ret);
        return ret;
    }

    ret = pwm_set_pulse_dt(&green_pwm, pulse_green);
    if (ret != 0) {
        printk("Error %d: green write failed\n", ret);
        return ret;
    }

    ret = pwm_set_pulse_dt(&blue_pwm, pulse_blue);
    if (ret != 0) {
        printk("Error %d: blue write failed\n", ret);
        return ret;
    }

    return 0;
}


int rgb_led_init(void) {
    if (!pwm_is_ready_dt(&red_pwm) ||
        !pwm_is_ready_dt(&green_pwm) ||
        !pwm_is_ready_dt(&blue_pwm)) {
        printk("Error: one or more PWM devices not ready\n");
        return -1;
    }

    // 默认熄灭，由上层状态指示模块接管配色
    rgb_led_set_color(0, 0, 0);
    return 0;
}

#include <zephyr/kernel.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/console/console.h>
#include <zephyr/sys/printk.h>
#include <stdlib.h>
#include <ctype.h>
#include "fan.h"


#define FAN_NODE DT_ALIAS(fan_pwm)

#if DT_NODE_EXISTS(FAN_NODE)
static const struct pwm_dt_spec pwm_dev = PWM_DT_SPEC_GET(FAN_NODE);
#else
#error "FAN device not found in DTS"
#endif



/* 
 * 18kHz 频率对应的周期约为 55556ns 
 * 1 sec / 18000 Hz = 55555.55... ns
 */
#define PERIOD_NS 55556U
#define MIN_DUTY  20
#define MAX_DUTY  100

#define FAN_CONTROL_STACK_SIZE 1024
static K_THREAD_STACK_DEFINE(fan_control_stack, FAN_CONTROL_STACK_SIZE);
static struct k_thread fan_control_thread;

void fan_control_thread_entry(void *p1, void *p2, void *p3)
{
    uint8_t duty_cycle = MIN_DUTY;

    while (1) {
        // 后续从算法和MQTT获取风速
        printk("Fan Control Thread Running\n");
        k_sleep(K_SECONDS(1));
    }
}

/* 设置 PWM 占空比 */
static int set_motor_pwm(uint8_t duty_cycle) {
    if (duty_cycle < MIN_DUTY) duty_cycle = MIN_DUTY;
    if (duty_cycle > MAX_DUTY) duty_cycle = MAX_DUTY;

    // 计算脉宽：pulse = period * duty / 100
    uint32_t pulse_ns = (PERIOD_NS * duty_cycle) / 100;
 
    int ret = pwm_set_dt(&pwm_dev, PERIOD_NS, pulse_ns);
    if (ret) {
        printk("Error %d: failed to set pulse width\n", ret);
        return ret;
    }
    printk("Fan Speed: %d%% (Pulse: %d ns)\n", duty_cycle, pulse_ns);
    return 0;
}

int fan_app_start(void) {
    if (!pwm_is_ready_dt(&pwm_dev)) {
		printk("Error: PWM device %s is not ready\n", pwm_dev.dev->name);
		return 0;
	}

    /* 上电默认设置为最小占空比 20% */
	set_motor_pwm(MIN_DUTY);

    k_thread_create(&fan_control_thread, fan_control_stack,
                    K_THREAD_STACK_SIZEOF(fan_control_stack),
                    fan_control_thread_entry,
                    NULL, NULL, NULL,
                    7, 0, K_NO_WAIT);

    return 0;
}
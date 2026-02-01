#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <stdio.h>

#include "aht10_app.h"

#define AHT10_SENSOR_STACK_SIZE 2048
static K_THREAD_STACK_DEFINE(aht10_sensor_stack, AHT10_SENSOR_STACK_SIZE);
static struct k_thread aht10_sensor_thread;

void aht10_sensor_thread_entry(void *p1, void *p2, void *p3)
{
    const struct device *dev = (const struct device *)p1;
    struct sensor_value temp, hum;

    while (1) {
        if (sensor_sample_fetch(dev) == 0) {
            sensor_channel_get(dev, SENSOR_CHAN_AMBIENT_TEMP, &temp);
            sensor_channel_get(dev, SENSOR_CHAN_HUMIDITY, &hum);
            #if SENSOR_CENTER_ENABLE
             // 通过传感器中心发送数据
             struct sensor_event ev = {
                .type = SENSOR_AHT10,
                .timestamp = k_uptime_get(),
                .data = {
                    .aht10 = {
                        .temp_x1000 = temp.val1 * 1000 + temp.val2 / 1000, // 转为 0.001°C
                        .hum_x1000 = hum.val1 * 1000 + hum.val2 / 1000,   // 转为 0.001%
                    }
                },
            };
            if (sensor_hub_send(&ev, K_NO_WAIT) != 0) {
                printk("Failed to send AHT10 data to sensor hub\n");
            }
            #else
            printk("AHT10 Temperature: %d.%06d C, Humidity: %d.%06d %%\n",
                   temp.val1, temp.val2, hum.val1, hum.val2);
            #endif
        } else {
            printk("Failed to fetch data from AHT10 sensor\n");
        }
        k_sleep(K_SECONDS(2));
    }
}


#define AHT10_NODE DT_ALIAS(aosong_aht10)

#if DT_NODE_EXISTS(AHT10_NODE)
static const struct device *aht10_dev = DEVICE_DT_GET(AHT10_NODE);
#else
#error "AHT10 device not found in DTS"
#endif

int aht10_app_start(void) {

    if (!device_is_ready(aht10_dev)) {
        printk("AHT10 not ready\n");
        return -1;
    }

    k_thread_create(&aht10_sensor_thread, aht10_sensor_stack,
                    K_THREAD_STACK_SIZEOF(aht10_sensor_stack),
                    aht10_sensor_thread_entry,
                    (void *)aht10_dev, NULL, NULL,
                    7, 0, K_NO_WAIT);
    return 0;
}
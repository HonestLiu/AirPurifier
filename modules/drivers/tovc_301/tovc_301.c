#include "tovc_301.h"

#define TOVC_301_NODE DT_ALIAS(tovc_301)
static const struct device *const tovc_dev = DEVICE_DT_GET(TOVC_301_NODE);

#define TVOC_FRAME_SIZE 9

/* 消息队列：存储 TVOC 传感器数据 */
K_MSGQ_DEFINE(tvoc_msgq, sizeof(tvoc_data_t), 10, 4);

#define TOVC_SENSOR_STACK_SIZE 512
static K_THREAD_STACK_DEFINE(tovc_sensor_stack, TOVC_SENSOR_STACK_SIZE);
static struct k_thread tovc_sensor_thread;

/**
 * @brief UART 中断回调：解析格式 2C E4 Y1 Y2 Y3 Y4 Y5 Y6 SUM [cite: 157]
 */
void tvoc_serial_cb(const struct device *dev, void *user_data)
{
    static uint8_t buf[TVOC_FRAME_SIZE];
    static int pos = 0;
    uint8_t c;

    if (!uart_irq_update(dev) || !uart_irq_rx_ready(dev)) return;

    while (uart_fifo_read(dev, &c, 1) == 1) {
        if (pos == 0) {
            if (c == 0x2C) buf[pos++] = c; // 帧头 [cite: 161]
        } else if (pos == 1) {
            if (c == 0xE4) buf[pos++] = c; // 保留位 [cite: 162]
            else pos = 0; 
        } else {
            buf[pos++] = c;
            if (pos == TVOC_FRAME_SIZE) {
                // 校验和 (B0+B1+...+B7) [cite: 167]
                uint8_t sum = 0;
                for (int i = 0; i < 8; i++) sum += buf[i];

                if (sum == buf[8]) {
                    tvoc_data_t data;
                    // 计算公式：High*256 + Low 
                    data.tvoc = ((uint16_t)buf[2] << 8) | buf[3];
                    data.hcho = ((uint16_t)buf[4] << 8) | buf[5];
                    data.eco2 = ((uint16_t)buf[6] << 8) | buf[7];
                    k_msgq_put(&tvoc_msgq, &data, K_NO_WAIT);
                }
                pos = 0;
            }
        }
    }
}

void tovc_sensor_thread_entry(void *p1, void *p2, void *p3)
{
    tvoc_data_t sensor_data;

    while (1) {
        if (k_msgq_get(&tvoc_msgq, &sensor_data, K_FOREVER) == 0) {

            #if SENSOR_CENTER_ENABLE
            // 发送到传感器中心
            struct sensor_event ev = {
                .type = SENSOR_TOVC_301,
                .timestamp = k_uptime_get(),
                .data.tvoc = sensor_data.tvoc,
                .data.hcho = sensor_data.hcho,
                .data.eco2 = sensor_data.eco2,
            }; 
            sensor_hub_send(&ev, K_NO_WAIT);
            #else
            // 如果要在 OLED 上显示 mg/m3，除以 1000 即可
            // 这里演示直接打印整数 (ug/m3)
            printk("[TVOC] %u ug/m3 | [HCHO] %u ug/m3 | [eCO2] %u ppm\n", 
                    sensor_data.tvoc, sensor_data.hcho, sensor_data.eco2);
            #endif
        }
    }
}

int tovc_sensor_app_start(void)
{
    if (!device_is_ready(tovc_dev)) {
        printk("Error: TOVC-301 UART device not ready\n");
        return -1;
    }

    /* 配置串口参数 9600 8N1 */
    struct uart_config uart_cfg = {
        .baudrate = 9600,
        .parity = UART_CFG_PARITY_NONE,
        .stop_bits = UART_CFG_STOP_BITS_1,
        .data_bits = UART_CFG_DATA_BITS_8,
        .flow_ctrl = UART_CFG_FLOW_CTRL_NONE,
    };
    int ret = uart_configure(tovc_dev, &uart_cfg);
    if (ret != 0) {
        printk("Error: TOVC-301 UART configuration failed: %d\n", ret);
        return -1;
    }

    /* 绑定回调并使能接收中断 */
    uart_irq_callback_user_data_set(tovc_dev, tvoc_serial_cb, NULL);
    uart_irq_rx_enable(tovc_dev);

    k_thread_create(&tovc_sensor_thread, tovc_sensor_stack,
                    K_THREAD_STACK_SIZEOF(tovc_sensor_stack),
                    tovc_sensor_thread_entry,
                    NULL, NULL, NULL,
                    7, 0, K_NO_WAIT);

    return 0;
}
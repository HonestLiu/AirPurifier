#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <string.h>
#include <zephyr/sys/printk.h>
#include "dc01_app.h"
    
/* 获取设备树别名定义的串口 */
#define DC01_NODE DT_ALIAS(dc_01)
static const struct device *const dc01_dev = DEVICE_DT_GET(DC01_NODE);

#define D01_FRAME_SIZE 4

/* 消息队列：存储 PM2.5 原始值 */
K_MSGQ_DEFINE(sensor_msgq, sizeof(uint16_t), 10, 4);

#define DC01_SENSOR_STACK_SIZE 512
static K_THREAD_STACK_DEFINE(dc01_sensor_stack, DC01_SENSOR_STACK_SIZE);
static struct k_thread dc01_sensor_thread;

/**
 * @brief UART 中断回调函数
 * 逻辑：滑动窗口解析 A5 XX XX SUM 格式
 */
static void dc01_serial_cb(const struct device *dev, void *user_data)
{
    static uint8_t temp_buf[D01_FRAME_SIZE];
    static int pos = 0;
    uint8_t c;

    if (!uart_irq_update(dev))
        return;
    if (!uart_irq_rx_ready(dev))
        return;

    while (uart_fifo_read(dev, &c, 1) == 1)
    {
        if (pos == 0)
        {
            if (c == 0xA5)
            { // 寻找同步头
                temp_buf[pos++] = c;
            }
        }
        else
        {
            temp_buf[pos++] = c;

            if (pos == D01_FRAME_SIZE)
            {
                // 校验计算
                uint8_t checksum = (temp_buf[0] + temp_buf[1] + temp_buf[2]) & 0x7F;

                if (checksum == temp_buf[3])
                {
                    // 计算 Raw 值: DATAH*128 + DATAL
                    uint16_t raw_val = ((uint16_t)(temp_buf[1] & 0x7F) << 7) | (temp_buf[2] & 0x7F);
                    k_msgq_put(&sensor_msgq, &raw_val, K_NO_WAIT);
                }
                pos = 0; // 无论校验是否成功，都重置位置找下一个 A5
            }
        }
    }
}

/**
 * @brief 从消息队列读取 DC01 原始 PM2.5 值
 * @param timeout 超时时间
 * @return 读取到的 PM2.5 原始值
 */
uint32_t read_dc01_raw_value(k_timeout_t timeout)
{
    uint16_t pm25_value;
    k_msgq_get(&sensor_msgq, &pm25_value, timeout);
    return pm25_value;
}

void dc01_sensor_thread_entry(void *p1, void *p2, void *p3)
{
    uint16_t pm25_raw;
    printk("--- DC01 PM2.5 Monitor (Fixed-Point Display) ---\n");
    while (1)
    {
        pm25_raw = read_dc01_raw_value(K_FOREVER);

        // 说明书公式：PM2.5 = (Raw * 4) / 10 
        struct sensor_event ev = {
            .type = SENSOR_DC01_PM25,
            .timestamp = k_uptime_get(),
            .data.pm25_raw_x10 = (uint32_t)pm25_raw * 4, // [此处以十倍精度发送到传感器中心，后续使用时再除以10]
        };

        sensor_hub_send(&ev, K_NO_WAIT);

        k_sleep(K_MSEC(1000));

        // /* 阻塞等待解析出的传感器数据 */
        // if (k_msgq_get(&sensor_msgq, &pm25_raw, K_FOREVER) == 0)
        // {

        //     /* * 整数模拟浮点逻辑：
        //      * 说明书公式：PM2.5 = Raw * 0.4
        //      * 我们可以转为：PM2.5 = (Raw * 4) / 10
        //      */
        //     uint32_t total_x10 = (uint32_t)pm25_raw * 4;
        //     uint32_t integer_part = total_x10 / 10; // 整数部分
        //     uint32_t decimal_part = total_x10 % 10; // 小数部分（十分位）

        //     /* 使用 %u.%u 格式拼接，避免使用 %f */
        //     printk("[DATA] Raw: %u | Concentration: %u.%u ug/m3\n",
        //            pm25_raw, integer_part, decimal_part);
        // }
    }
}

int dc01_sensor_app_start(void)
{

    if (!device_is_ready(dc01_dev))
    {
        printk("Error: DC01 UART device not ready\n");
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
    int ret = uart_configure(dc01_dev, &uart_cfg);
    if (ret != 0)
    {
        printk("Error: DC01 UART configuration failed: %d\n", ret);
        return -1;
    }

    /* 绑定回调并使能接收中断 */
    uart_irq_callback_user_data_set(dc01_dev, dc01_serial_cb, NULL);
    uart_irq_rx_enable(dc01_dev);
    

    k_thread_create(&dc01_sensor_thread, dc01_sensor_stack,
                    K_THREAD_STACK_SIZEOF(dc01_sensor_stack),
                    dc01_sensor_thread_entry,
                    NULL, NULL, NULL,
                    7, 0, K_NO_WAIT);

    return 0;
}
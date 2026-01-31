#define DT_DRV_COMPAT kunliu_dc01

#include "dc01.h"
#include <zephyr/logging/log.h>
#include <zephyr/drivers/uart.h>

LOG_MODULE_REGISTER(dc01, CONFIG_SENSOR_LOG_LEVEL);

#define D01_FRAME_SIZE 4

/**
 * @brief UART 中断服务程序 (移花接木版本)
 * @note 逻辑完全复刻自用户提供的验证成功的应用层代码
 */
static void d01_uart_isr(const struct device *dev, void *user_data)
{
    struct d01_data *data = user_data;
    uint8_t c;

    /* 必须先 update 才能读取状态 */
    if (!uart_irq_update(dev)) return;

    /* 检查是否有 RX 数据就绪 (不同硬件实现可能略有不同) */
    if (!uart_irq_rx_ready(dev)) return;
    
    data->isr_cnt++; // Debug: 进入 ISR

    /* 循环读取直到 FIFO 为空 */
    while (uart_fifo_read(dev, &c, 1) == 1) {
        data->rx_cnt++; // Debug: 收到字节

        if (data->rx_pos == 0) {
            if (c == 0xA5) { // 寻找同步头
                data->rx_buf[data->rx_pos++] = c;
            }
        } else {
            data->rx_buf[data->rx_pos++] = c;
            
            if (data->rx_pos == D01_FRAME_SIZE) {
                // 校验计算
                uint8_t checksum = (data->rx_buf[0] + data->rx_buf[1] + data->rx_buf[2]) & 0x7F;
                
                if (checksum == data->rx_buf[3]) {
                    /* 解析成功：计算 Raw 值 */
                    uint16_t raw_val = ((uint16_t)(data->rx_buf[1] & 0x7F) << 7) | (data->rx_buf[2] & 0x7F);
                    
                    /* 发送到消息队列 (非阻塞，满了就丢弃旧的或者直接失败均可，这里用 purge 策略保证最新?) 
                     * 简单起见，使用 K_NO_WAIT。如果满了，sensor 线程没来得及读，那丢了也无所谓
                     */
                   k_msgq_put(&data->rx_msgq, &raw_val, K_NO_WAIT);
                }
                data->rx_pos = 0; // 重置状态机
            }
        }
    }
}

/**
 * @brief 获取传感器数据
 * @note 只需要从队列里取出一个最新的有效值即可
 */
static int d01_sample_fetch(const struct device *dev, enum sensor_channel chan)
{
    struct d01_data *drv_data = dev->data;
    static uint32_t fetch_cnt = 0;

    // 清空队列里堆积的旧数据，只取最后一个
    uint16_t val;
    int ret = k_msgq_get(&drv_data->rx_msgq, &val, K_NO_WAIT);
    
    if (ret != 0) {
        fetch_cnt++;
        if (fetch_cnt % 10 == 0) { // 每调用10次打印一次
             /* 如果 isr_cnt 不涨，说明没中断 --> 硬件或 pinctrl 问题 */
             /* 如果 isr_cnt 涨但 rx_cnt 不涨 --> uart_fifo_read 问题 */
             /* 如果 rx_cnt 涨但还是 EAGAIN --> 解析/校验失败或头不对 */
             const struct d01_config *config = dev->config;
             LOG_WRN("No data. UART: %s, ISR: %u, Bytes: %u", 
                     config->uart_dev->name, drv_data->isr_cnt, drv_data->rx_cnt);
        }
        return -EAGAIN; 
    }
    
    fetch_cnt = 0;
    while (k_msgq_get(&drv_data->rx_msgq, &val, K_NO_WAIT) == 0) {
        // flush old data
    }

    drv_data->pm25_raw = val;
    return 0;
}

/* --- 输出数据 (转为传感器标准格式 val1.val2) --- */
static int d01_channel_get(const struct device *dev, enum sensor_channel chan, 
                            struct sensor_value *val)
{
    struct d01_data *drv_data = dev->data;

    if (chan == SENSOR_CHAN_PM_2_5) {
        /* Formula: Concentration = raw * 0.4 */
        uint32_t total_micro = (uint32_t)drv_data->pm25_raw * 400000U; 
        val->val1 = total_micro / 1000000U;
        val->val2 = total_micro % 1000000U;
        return 0;
    }
    return -ENOTSUP;
}

static const struct sensor_driver_api d01_api = {
    .sample_fetch = d01_sample_fetch,
    .channel_get = d01_channel_get,
};

static int d01_init(const struct device *dev)
{
    struct d01_data *drv_data = dev->data;
    const struct d01_config *config = dev->config;
    const struct device *uart_dev = config->uart_dev;

    if (!device_is_ready(uart_dev)) return -ENODEV;

    struct uart_config uart_cfg = {
        .baudrate = 9600,
        .parity = UART_CFG_PARITY_NONE,
        .stop_bits = UART_CFG_STOP_BITS_1,
        .data_bits = UART_CFG_DATA_BITS_8,
        .flow_ctrl = UART_CFG_FLOW_CTRL_NONE,
    };
    uart_configure(uart_dev, &uart_cfg);

    /* 重置所有数据结构 */
    /* mem初始化 msgq */
    k_msgq_init(&drv_data->rx_msgq, drv_data->rx_msgq_buffer, 
                sizeof(uint16_t), 10);

    /* 重置 ISR 状态机 */
    drv_data->rx_pos = 0;

    LOG_INF("DC01 init on %s. 9600-8N1", uart_dev->name);

    uart_irq_callback_user_data_set(uart_dev, d01_uart_isr, drv_data);
    uart_irq_rx_enable(uart_dev);

    LOG_INF("DC01 Driver Loaded (ISR Parsing Mode)");
    return 0;
}

#define D01_DEFINE(inst) \
    static const struct d01_config d01_cfg_##inst = { \
        .uart_dev = DEVICE_DT_GET(DT_INST_BUS(inst)), \
    }; \
    static struct d01_data d01_data_##inst; \
    SENSOR_DEVICE_DT_INST_DEFINE(inst, d01_init, NULL, \
                                 &d01_data_##inst, &d01_cfg_##inst, \
                                 POST_KERNEL, CONFIG_SENSOR_INIT_PRIORITY, &d01_api);

DT_INST_FOREACH_STATUS_OKAY(D01_DEFINE)

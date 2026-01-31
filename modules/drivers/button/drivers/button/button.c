// 指定此驱动与设备树中 compatible = "custom,button" 的节点匹配
#define DT_DRV_COMPAT custom_button

#include <errno.h>               // 标准错误码定义（如 -ENODEV）
#include <zephyr/logging/log.h>  // Zephyr 日志系统头文件

#include "button.h"              // 按钮驱动的头文件

LOG_MODULE_REGISTER(button); // 注册日志模块，日志级别为调试

static int button_init(const struct device *dev);                      // 驱动初始化函数
static int button_state_get(const struct device *dev, uint8_t *state); // 读取按钮状态


/**
 *  @brief 按钮驱动初始化函数
 *  @param dev 指向设备结构体的指针
 *  @return 0 表示成功，负值表示失败
 */
static int button_init(const struct device *dev) {
    int ret;

    // 将通用的 dev->config (void*) 转换为本驱动专用的配置结构体
    const struct button_config *cfg = (const struct button_config *)dev->config;

    // 从配置中提取 GPIO 规范（包含控制器、引脚号、标志等）
    const struct gpio_dt_spec *btn = &cfg->btn;

    // 打印调试信息：显示当前初始化的是第几个按钮实例（支持多实例）
    LOG_DBG("Initializing button (instance ID: %u)\r\n", cfg->id);

    // 检查 GPIO 控制器设备是否已准备好（例如，对应的 GPIO 驱动是否已初始化）
    if (!gpio_is_ready_dt(btn)) {
        LOG_ERR("GPIO is not ready\r\n");
        return -ENODEV;  // 返回“无此设备”错误
    }

    // 配置该 GPIO 引脚为输入模式（可根据需要添加上拉/下拉等标志）
    ret = gpio_pin_configure_dt(btn, GPIO_INPUT);
    if (ret < 0) {
        LOG_ERR("Could not configure GPIO as input\r\n");
        return -ENODEV;
    }

    // 初始化成功
    return 0;
}

/**
 *  @brief 获取按钮当前状态
 *  @param dev 指向设备结构体的指针
 *  @param state 指向存储按钮状态的变量的指针（0 或 1）
 *  @return 0 表示成功，负值表示失败
 */
static int button_state_get(const struct device *dev, uint8_t *state)
{
    int ret;

    // 从设备结构体中获取配置信息
    const struct button_config *cfg = (const struct button_config *)dev->config;
    const struct gpio_dt_spec *btn = &cfg->btn;

    // 读取 GPIO 引脚当前电平
    ret = gpio_pin_get_dt(btn);
    if (ret < 0) {
        // 如果读取失败（如引脚无效），记录错误并返回错误码
        LOG_ERR("Error (%d): failed to read button pin\r\n", ret);
        return ret;
    } else {
        // 成功读取，将结果（0 或 1）存入输出参数
        *state = (uint8_t)ret;
    }

    return 0;
}

// 定义按钮驱动的 API 结构体，包含驱动支持的函数指针
static const struct button_api button_api_funcs = {
    .get = button_state_get,  // 应用调用 dev->api->get(...) 时实际执行此函数
};

#define BUTTON_DEFINE(inst)                                                     \
                                                                                \
    /* 1. 创建配置结构体实例 */                                                 \
    static const struct button_config button_config_##inst = {                  \
        /* 从设备树节点中提取 'pin' 属性所指向的 gpios 信息 */                  \
        .btn = GPIO_DT_SPEC_GET(                                                \
            DT_PHANDLE(DT_INST(inst, custom_button), pin), gpios),              \
        .id = inst  /* 记录实例编号，用于调试 */                                \
    };                                                                          \
                                                                                \
    /* 2. 向 Zephyr 内核注册该设备实例 */                                       \
    DEVICE_DT_INST_DEFINE(inst,                                                 \
                          button_init,       /* 初始化函数 */                   \
                          NULL,              /* PM device（电源管理，未用*/     \
                          NULL,              /* 运行时数据（本例无需）*/        \
                          &button_config_##inst, /* 指向上述配置 */             \
                          POST_KERNEL,       /* 初始化阶段：内核之后 */         \
                          CONFIG_GPIO_INIT_PRIORITY, /* 优先级：同 GPIO 驱动 */ \
                          &button_api_funcs);/* 指向 API 函数表 */              \

// 遍历所有状态为 "okay" 的 custom,button 设备树实例，
// 并为每个实例调用 BUTTON_DEFINE 宏，从而自动生成所有必要代码。
DT_INST_FOREACH_STATUS_OKAY(BUTTON_DEFINE)
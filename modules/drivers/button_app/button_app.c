#include "button_app.h"

#include "button.h"
#include "control_center.h"

static const int32_t sleep_time_ms = 50;

#define BUTTON_NODE DT_ALIAS(key)

#if DT_NODE_EXISTS(BUTTON_NODE)
static const struct device *const btn_1 = DEVICE_DT_GET(BUTTON_NODE);
#else
#error "Button device not found in DTS"
#endif

#define BUTTON_STACK_SIZE 512
K_THREAD_STACK_DEFINE(button_stack, BUTTON_STACK_SIZE);
struct k_thread button_thread;

void button_thread_func(void *a, void *b, void *c) {
    uint8_t state_1;
    uint8_t last_state = 0xFF; // 初始化为无效状态，确保第一次读取时触发事件

    if (!device_is_ready(btn_1)) {
        printk("Button device not ready\n");
        return;
    }

    const struct button_api *btn_api = (const struct button_api *) btn_1->api;

    printk("[Button App] Button thread started\n");

    while (1) {
        int ret = btn_api->get(btn_1, &state_1);
        if (ret < 0) {
            printk("Error (%d): failed to read button 1 pin\r\n", ret);
            continue;
        }

        if (state_1 != last_state) {
            if (state_1) {
                printk("[Button App] Button Pressed\n");
                control_toggle_fan_power();
            } else {
                printk("[Button App] Button Released\n");
                // 在此处添加释放按钮时的处理逻辑
            }
            last_state = state_1;
        }

        k_sleep(K_MSEC(sleep_time_ms));
    }
}

int button_app_start(void) {
    k_thread_create(&button_thread,
                    button_stack,
                    K_THREAD_STACK_SIZEOF(button_stack),
                    button_thread_func,
                    NULL, NULL, NULL,
                    5, // 优先级
                    0, // 无特殊选项
                    K_NO_WAIT); // 立即启动
    return 0;
}

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <u8g2.h>
#include "u8g2_zephyr_port.h"

#include "gui_app.h"
#include "wifi_prov_app.h"
#include "app_mqtt.h"

/* 建议：增大 GUI 线程栈大小，u8g2 图形库比较消耗栈空间 */
#define GUI_STACK_SIZE 2048
K_THREAD_STACK_DEFINE(gui_stack, GUI_STACK_SIZE);

/* 修复：使用不同的变量名，避免与 gui_thread 函数名冲突 */
struct k_thread gui_thread_data;

int main(void)
{
    printk("Air Purifier Application Start\n");

    /* 1. 先创建并启动 GUI 线程，确保上电屏幕立刻显示 */
    k_tid_t gui_tid = k_thread_create(&gui_thread_data, gui_stack,
                                      K_THREAD_STACK_SIZEOF(gui_stack),
                                      gui_thread_func,
                                      NULL, NULL, NULL,
                                      5, 0, K_NO_WAIT);
    
    printk("GUI thread created with TID: %p\n", gui_tid);

    /* 2. 这里的休眠只影响配网启动，不会阻塞 GUI 显示了 */
    printk("Starting WiFi Provisioning Service...\n");
    wifi_prov_app_start();

    /* 3. 等待配网 */
    int64_t start_time = k_uptime_get(); // 记录开始时间
    // 每5秒检查一次，直到连接成功或超时
    while (k_sem_take(&wifi_connected_sem, K_SECONDS(5)) != 0)
    {
        if(k_uptime_get() - start_time > K_HOURS(1).ticks * CONFIG_SYS_CLOCK_TICKS_PER_SEC / 1000) {
            printk("WiFi Provisioning Timeout. Restarting...\n");
            break; // 超时，跳出等待
        }
        printk("Waiting for WiFi connection...\n");
        // 闪烁LED、喂看门狗
    }

    // 4. 启动 MQTT 客户端
    app_mqtt_start();



    while (1)
    {
        k_msleep(1000);
    }

    return 0;
}
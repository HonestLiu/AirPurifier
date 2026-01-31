#include "app_mqtt.h"

// ===== 回调函数 =====

// 收到消息的回调
static void on_mqtt_message(const char *topic, const void *payload, size_t len)
{
    printk("[App] Recv Topic: %s\n", topic);

    // 可以在此处直接解析，或放入队列处理
    // 为了简单演示，直接解析 (注意 payload 需要是 null 结尾的字符串，
    // mqtt_backend 传递过来的 payload 是 raw bytes。
    // 但是 json_obj_parse 需要字符串长度，不强制 null 结尾，但为了 %s 打印方便，我们处理一下)
    
    // 如果 topic 匹配
    if (strcmp(topic, TOPIC_SUB) == 0) {
        struct control_cmd cmd;
        memset(&cmd, 0, sizeof(cmd));
        cmd.fan_speed = -1;

        // 临时 buffer 用于确保 JSON 格式安全 (可选)
        // json_obj_parse 在 Zephyr 中通常直接操作输入 buffer
        int ret = json_obj_parse((char *)payload, len, 
                                 control_descr, ARRAY_SIZE(control_descr), 
                                 &cmd);
        
        if (ret >= 0) {
            printk(">>> [App] CMD Executed <<<\n");
            printk("  Power: %s\n", cmd.power ? "ON" : "OFF");
            if (cmd.fan_speed != -1) printk("  Fan: %d\n", cmd.fan_speed);
            if (cmd.mode)            printk("  Mode: %s\n", cmd.mode);
        } else {
            printk("[App] JSON Error: %d\n", ret);
        }
    }
}

// ===== 上报线程 =====
#define REPORTER_STACK_SIZE 2048
static K_THREAD_STACK_DEFINE(reporter_stack, REPORTER_STACK_SIZE);
static struct k_thread reporter_data;

void reporter_thread_entry(void *p1, void *p2, void *p3)
{
    char buffer[128];
    double temp = 23.5;
    int hum = 45;
    
    printk("[App] Reporter Thread Started\n");

    while (1) {
        k_sleep(K_SECONDS(5));

        // 构造 JSON
        // 注意：CONFIG_CBPRINTF_FP_SUPPORT=y 必须开启才能正确打印浮点
        snprintf(buffer, sizeof(buffer), 
            "{\"temp\": %.1f, \"hum\": %d, \"status\": \"ok\"}",
            temp, hum);

        // 调用 backend 发布
        int ret = mqtt_backend_publish(TOPIC_PUB_DATA, buffer, strlen(buffer));
        if (ret == 0) {
            printk("[App] Pub: %s\n", buffer);
        } else {
            // printk("[App] Pub failed (not connected)\n");
        }

        // 模拟变化
        temp += 0.5;
        if (temp > 35.0) temp = 20.0;
        hum++;
        if (hum > 90) hum = 30;
    }
}

// ===== 启动接口 =====
void app_mqtt_start(void)
{
    static struct mqtt_backend_config config = {
        .broker_ip = BROKER_IP,
        .broker_port = BROKER_PORT,
        .client_id = CLIENT_ID,
        .user = USERNAME,
        .pass = PASSWORD,
        .sub_topics = sub_topics,
        .on_msg_cb = on_mqtt_message
    };

    // 启动 backend
    mqtt_backend_start(&config);

    // 启动上报线程
    k_thread_create(&reporter_data, reporter_stack, K_THREAD_STACK_SIZEOF(reporter_stack),
                    reporter_thread_entry, NULL, NULL, NULL,
                    7, 0, K_NO_WAIT);
}

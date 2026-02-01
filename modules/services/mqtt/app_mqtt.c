#include "app_mqtt.h"
#include <stdio.h>
#include "../control_center/control_center.h"

// ===== 回调函数 =====

// 收到消息的回调
static void on_mqtt_message(const char *topic, const void *payload, size_t len)
{
    printk("[App] Recv Topic: %s\n", topic);
    
    // 如果 topic 匹配
    if (strcmp(topic, TOPIC_SUB) == 0) {
        struct control_cmd cmd;
        memset(&cmd, 0, sizeof(cmd));
        cmd.fan_speed = -1; // -1 indicates not set
        cmd.mode = NULL;    // NULL indicates not set

        // 下发 {"fan_speed": "low"/"off".., "mode": "auto"/"manual"..}
        int ret = json_obj_parse((char *)payload, len, 
                                 control_descr, ARRAY_SIZE(control_descr), 
                                 &cmd);
        
        if (ret >= 0) {
            printk(">>> [App] CMD Recv <<<\n");
            
            // 模式控制
            if (cmd.mode && strlen(cmd.mode) > 0) {
                control_set_mode(cmd.mode);
            }
            // 简单处理：若结构体里只有int fan_speed，则目前无法处理字符串 "low"
            // 若用户真的发了数字:
             if (cmd.fan_speed != -1) {
                 // 暂时转为字符串调通用接口，或者 control_center 增加 int 接口
                 const char* s = (cmd.fan_speed == 0) ? "off" : 
                                 (cmd.fan_speed == 1) ? "low" :
                                 (cmd.fan_speed == 2) ? "medium" : "high";
                 control_set_fan_cmd(s);
             }
        } 
    }
}

// ===== 上报线程 =====
#define REPORTER_STACK_SIZE 2048
static K_THREAD_STACK_DEFINE(reporter_stack, REPORTER_STACK_SIZE);
static struct k_thread reporter_data;

void reporter_thread_entry(void *p1, void *p2, void *p3)
{
    char buffer[256];
    air_purifier_status_t status;
    
    printk("[App] Reporter Thread Started\n");

    while (1) {
        k_sleep(K_SECONDS(5));

        // 获取最新状态
        control_get_status(&status);

        // Map enum to string
        const char* mode_str = (status.mode == MODE_AUTO) ? "auto" : 
                               (status.mode == MODE_NIGHT) ? "night" : "manual";
        const char* speed_str = (status.fan_speed_enum == 0) ? "off" :
                                (status.fan_speed_enum == 1) ? "low" :
                                (status.fan_speed_enum == 2) ? "medium" : "high";

        // 构造 JSON
        snprintf(buffer, sizeof(buffer), 
            "{\"device_id\": \"%s\", \"timestamp\": %lld, \"mode\": \"%s\", \"fan_speed\": \"%s\", "
            "\"sensors\": {\"pm25\": %u, \"tvoc\": %u, \"hcho\": %u, \"eco2\": %u, \"temp\": %.1f, \"hum\": %.1f}, "
            "\"warnings\": [%s%s]}",
            CLIENT_ID, k_uptime_get(), mode_str, speed_str,
            status.pm25_val, status.tvoc_val, status.hcho_val, status.eco2_val, status.temp_val, status.hum_val,
            status.alert_high_pollution ? "\"high_pollution\"," : "",
            status.alert_replace_filter ? "\"replace_filter\"" : "\"none\""
        );
        
        // Clean up trailing comma in warnings if needed or just leave valid JSON
        // Simple valid json hack: always append "None" if empty? Or just let the code above handle.
        // The above logic: ["high_pollution","none"] or ["none"] is safer.
        // Refined: warning list logic is a bit complex for snprintf. simplified above.

        int ret = mqtt_backend_publish(TOPIC_PUB_DATA, buffer, strlen(buffer));
        if (ret == 0) {
            printk("[App] Pub: %s\n", buffer);
        }
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

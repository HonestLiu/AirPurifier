#ifndef APP_MQTT_H
#define APP_MQTT_H

#include <zephyr/kernel.h>
#include <zephyr/data/json.h>
#include <stdio.h>
#include <string.h>
#include "mqtt_backend.h"

// ===== 业务配置 =====
#define BROKER_IP       "192.168.31.191"
#define BROKER_PORT     1883
#define CLIENT_ID       "esp32s3_app_logic"
#define USERNAME        "your_user"
#define PASSWORD        "your_password"

#define TOPIC_SUB       "device/esp32/command"
#define TOPIC_PUB_DATA  "sensor/data"

// ===== JSON 解析结构 =====
struct control_cmd {
    bool power;
    int fan_speed;
    char *mode;
};

static const struct json_obj_descr control_descr[] = {
    JSON_OBJ_DESCR_PRIM(struct control_cmd, power, JSON_TOK_TRUE),
    JSON_OBJ_DESCR_PRIM(struct control_cmd, fan_speed, JSON_TOK_NUMBER),
    JSON_OBJ_DESCR_PRIM(struct control_cmd, mode, JSON_TOK_STRING),
};

// ===== 全局变量 =====
// 定义订阅列表 (可自由添加，但最后一个要是 NULL )
static const char *sub_topics[] = {
    TOPIC_SUB,
    NULL
};

/**
 * @brief 启动应用层 MQTT 业务
 *        (包含连接、订阅、数据解析、自动上报等业务逻辑)
 */
void app_mqtt_start(void);

#endif /* APP_MQTT_H */

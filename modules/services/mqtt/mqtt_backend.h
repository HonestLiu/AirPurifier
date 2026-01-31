#ifndef MQTT_BACKEND_H
#define MQTT_BACKEND_H

#include <zephyr/kernel.h>
#include <stddef.h>


/**
 * @brief MQTT 接收消息回调函数类型
 * @param topic 消息的主题
 * @param payload 消息内容（二进制数据）
 * @param len 内容长度
 */
typedef void (*mqtt_backend_msg_cb_t)(const char *topic, const void *payload, size_t len);

/**
 * @brief MQTT 服务配置结构体
 */
struct mqtt_backend_config {
    const char *broker_ip;
    uint16_t broker_port;
    const char *client_id;
    
    // 认证信息 (可选，设为 NULL 则不使用)
    const char *user;
    const char *pass;

    // 订阅列表，以 NULL 结尾的字符串数组
    // 例如: { "topic/a", "topic/b", NULL }
    const char **sub_topics;

    // 收到消息时的回调函数
    mqtt_backend_msg_cb_t on_msg_cb;
};

/**
 * @brief 启动 MQTT 后端服务
 *        会自动管理连接、重连、心跳以及订阅
 * @param config 配置参数
 * @return 0 成功
 */
int mqtt_backend_start(const struct mqtt_backend_config *config);

/**
 * @brief 发布消息
 * @param topic 主题
 * @param payload 内容
 * @param len 内容长度 (如果是字符串，通常是 strlen)
 * @return 0 成功
 */
int mqtt_backend_publish(const char *topic, const void *payload, size_t len);

#endif /* MQTT_BACKEND_H */

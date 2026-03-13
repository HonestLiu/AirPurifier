#include <zephyr/kernel.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/mqtt.h>
#include <string.h>
#include "mqtt_backend.h"

// ===== 内部配置常量 =====
#define NETWORK_TIMEOUT_MS      3000
#define KEEP_ALIVE_SECONDS      60
#define RX_TX_BUFFER_SIZE       256
#define THREAD_STACK_SIZE       2048
#define THREAD_PRIORITY         7
#define RECONNECT_DELAY_MS      2000

// ===== 内部全局变量 =====
static uint8_t rx_buffer[RX_TX_BUFFER_SIZE];
static uint8_t tx_buffer[RX_TX_BUFFER_SIZE];

static struct mqtt_client client;
static struct sockaddr_storage broker;
static bool connected = false;

// 互斥锁保护 client 操作
K_MUTEX_DEFINE(backend_lock);

// 配置副本
static struct mqtt_backend_config current_config;

// 线程定义
static K_THREAD_STACK_DEFINE(rx_stack, THREAD_STACK_SIZE);
static struct k_thread rx_thread_data;


// ===== 内部函数声明 =====
static void mqtt_evt_handler(struct mqtt_client *const c, const struct mqtt_evt *evt);

static int subscribe_list(void);

// ===== 实现 =====

/**
 * @brief 订阅配置中的主题列表
 */
static int subscribe_list(void) {
    // 没有订阅列表则直接返回
    if (!current_config.sub_topics) return 0;

    int i = 0;
    while (current_config.sub_topics[i] != NULL) {
        const char *topic_str = current_config.sub_topics[i];

        struct mqtt_topic topic = {
            .topic = {
                .utf8 = (uint8_t *) topic_str,
                .size = strlen(topic_str)
            },
            .qos = MQTT_QOS_0_AT_MOST_ONCE
        };

        struct mqtt_subscription_list sub_list = {
            .list = &topic,
            .list_count = 1,
            .message_id = k_uptime_get_32()
        };

        printk("[Backend] Subscribing to: %s\n", topic_str);
        int ret = mqtt_subscribe(&client, &sub_list);
        if (ret != 0) {
            printk("[Backend] Subscribe failed: %d\n", ret);
        }

        k_sleep(K_MSEC(100)); // 防止发送太快
        i++;
    }
    return 0;
}

/**
 * @brief MQTT 事件回调处理
 * @param c MQTT 客户端实例
 * @param evt 事件数据
 */
static void mqtt_evt_handler(struct mqtt_client *const c, const struct mqtt_evt *evt) {
    switch (evt->type) {
        case MQTT_EVT_CONNACK:
            if (evt->result == 0) {
                connected = true;
                printk("[Backend] Connected!\n");
                subscribe_list();
            } else {
                printk("[Backend] Connection refused: %d\n", evt->result);
            }
            break;

        case MQTT_EVT_DISCONNECT:
            connected = false;
            printk("[Backend] Disconnected\n");
            break;

        case MQTT_EVT_PUBLISH: {
            const struct mqtt_publish_param *p = &evt->param.publish;

            // 读取 Payload
            uint8_t buf[128]; // 临时 buffer
            int len = p->message.payload.len;
            int read_len = mqtt_read_publish_payload(c, buf,
                                                     len > sizeof(buf) - 1 ? sizeof(buf) - 1 : len);

            if (read_len >= 0) {
                // 调用回调 (需要确保 topic 以 null 结尾，这里构造一个临时 topic 串)
                if (current_config.on_msg_cb) {
                    // Topic 不是 null 结尾的，需要复制
                    char topic_buf[64];
                    size_t tlen = p->message.topic.topic.size;
                    if (tlen >= sizeof(topic_buf)) tlen = sizeof(topic_buf) - 1;
                    memcpy(topic_buf, p->message.topic.topic.utf8, tlen);
                    topic_buf[tlen] = '\0';

                    // 调用上层回调(应用层实现，负责具体解析并处理)
                    current_config.on_msg_cb(topic_buf, buf, read_len);
                }
            }
            break;
        }

        case MQTT_EVT_PINGRESP:
            // printk("[Backend] Ping Resp\n");
            break;

        default:
            break;
    }
}

/**
 * @brief 后端接收线程入口
 */
void backend_rx_thread_entry(void *p1, void *p2, void *p3) {
    int ret;
    struct zsock_pollfd fds[1];

    printk("[Backend] Thread Started\n");

    while (1) {
        if (!connected) {
            k_mutex_lock(&backend_lock, K_FOREVER);
            ret = mqtt_connect(&client);
            k_mutex_unlock(&backend_lock);

            if (ret != 0) {
                printk("[Backend] Connect fail (%d), retrying...\n", ret);
                k_sleep(K_MSEC(RECONNECT_DELAY_MS));
                continue;
            }

            // 等待连接确认
            fds[0].fd = client.transport.tcp.sock;
            fds[0].events = ZSOCK_POLLIN;
            if (zsock_poll(fds, 1, NETWORK_TIMEOUT_MS) > 0) {
                k_mutex_lock(&backend_lock, K_FOREVER);
                mqtt_input(&client);
                k_mutex_unlock(&backend_lock);
            }

            if (!connected) {
                mqtt_abort(&client); // 没连上就重置
                k_sleep(K_MSEC(1000));
            }
            continue;
        }

        // 正常轮询
        fds[0].fd = client.transport.tcp.sock;
        fds[0].events = ZSOCK_POLLIN;
        ret = zsock_poll(fds, 1, 1000); // 1s 超时

        if (ret > 0 && (fds[0].revents & ZSOCK_POLLIN)) {
            k_mutex_lock(&backend_lock, K_FOREVER);
            mqtt_input(&client);
            k_mutex_unlock(&backend_lock);
        }

        // 心跳
        k_mutex_lock(&backend_lock, K_FOREVER);
        mqtt_live(&client);
        k_mutex_unlock(&backend_lock);
    }
}

/**
 * @brief 启动 MQTT 后端
 * @param config 配置参数
 * @return 0 成功
 */
int mqtt_backend_start(const struct mqtt_backend_config *config) {
    if (!config || !config->broker_ip) return -EINVAL;

    // 保存配置
    current_config = *config;

    // 初始化 Client
    mqtt_client_init(&client);

    struct sockaddr_in *broker4 = (struct sockaddr_in *) &broker;
    broker4->sin_family = AF_INET;
    broker4->sin_port = htons(config->broker_port);
    zsock_inet_pton(AF_INET, config->broker_ip, &broker4->sin_addr);

    client.broker = &broker;
    client.evt_cb = mqtt_evt_handler;
    client.client_id.utf8 = (uint8_t *) config->client_id;
    client.client_id.size = strlen(config->client_id);
    client.protocol_version = MQTT_VERSION_3_1_1;
    client.transport.type = MQTT_TRANSPORT_NON_SECURE;
    client.keepalive = KEEP_ALIVE_SECONDS;

    client.rx_buf = rx_buffer;
    client.rx_buf_size = sizeof(rx_buffer);
    client.tx_buf = tx_buffer;
    client.tx_buf_size = sizeof(tx_buffer);

    // 认证配置
    if (config->user && config->pass) {
        static struct mqtt_utf8 user_u, pass_u;
        user_u.utf8 = (uint8_t *) config->user;
        user_u.size = strlen(config->user);
        pass_u.utf8 = (uint8_t *) config->pass;
        pass_u.size = strlen(config->pass);
        client.user_name = &user_u;
        client.password = &pass_u;
    }

    // 启动线程
    k_thread_create(&rx_thread_data, rx_stack, K_THREAD_STACK_SIZEOF(rx_stack),
                    backend_rx_thread_entry, NULL, NULL, NULL,
                    THREAD_PRIORITY, 0, K_NO_WAIT);

    return 0;
}

/**
 * @brief 发布消息
 * @param topic 主题
 * @param payload 内容
 * @param len 内容长度 (如果是字符串，通常是 strlen)
 * @return 0 成功
 */
int mqtt_backend_publish(const char *topic, const void *payload, size_t len) {
    if (!connected) return -ENOTCONN;

    struct mqtt_publish_param param;
    param.message.topic.qos = MQTT_QOS_0_AT_MOST_ONCE;
    param.message.topic.topic.utf8 = (uint8_t *) topic;
    param.message.topic.topic.size = strlen(topic);
    param.message.payload.data = (uint8_t *) payload;
    param.message.payload.len = len;
    param.message_id = k_uptime_get_32();
    param.dup_flag = 0U;
    param.retain_flag = 0U;

    int ret;
    k_mutex_lock(&backend_lock, K_FOREVER);
    ret = mqtt_publish(&client, &param);
    k_mutex_unlock(&backend_lock);

    return ret;
}

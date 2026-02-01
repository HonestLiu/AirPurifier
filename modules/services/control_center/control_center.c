#include "control_center.h"
#include <string.h>
#include <zephyr/sys/printk.h>
#include "gui.h"
#include "fan.h"
#include "status_indicator.h"

// --- 消息定义 ---
typedef enum {
    CTRL_EVT_PM25,      // PM2.5 数据
    CTRL_EVT_ENV,       // TVOC, HCHO, eCO2 数据
    CTRL_EVT_TH,        // 温湿度数据
    CTRL_EVT_WIFI,      // WiFi 状态
    CTRL_EVT_CMD_MODE,  // 设置模式命令
    CTRL_EVT_CMD_FAN,   // 设置风速命令
    CTRL_EVT_FAN_POWER_TOGGLE // 风扇电源开关
} ctrl_evt_type_t;

typedef struct {
    ctrl_evt_type_t type;                               // 事件类型
    union { 
        uint32_t pm25;                                  // PM2.5 数据
        struct { uint16_t tvoc, hcho, eco2; } env;      // 环境数据
        struct { float temp, hum; } th;                 // 温湿度数据
        int  fan_speed_enum;                            // 风速枚举
        char mode_str[16];                              // 模式字符串
        bool wifi_connected;                            // WiFi 连接状态
    } data;
} ctrl_msg_t;

K_MSGQ_DEFINE(control_msgq, sizeof(ctrl_msg_t), 20, 4);

// 全局状态
static air_purifier_status_t g_status = {
    .mode = MODE_AUTO,                                              // 默认自动模式
    .wifi_connected = false,                                       // 默认未连接
    .fan_speed_enum = 0,                                            // 默认风速OFF
    .fan_power_enabled = true,
    .filter_life_hours = 0,                                         // 过滤器寿命
    .alert_high_pollution = false,                                  // 默认无警告
    .alert_replace_filter = false,                                  // 默认无警告
    .pm25_val = 0, .tvoc_val = 0, .hcho_val = 0, .eco2_val = 400,   // 默认环境值
    .temp_val = 25.0f, .hum_val = 50.0f
};

static fan_speed_t sanitize_speed(int speed_level) {
    if (speed_level < FAN_SPEED_OFF) {
        return FAN_SPEED_OFF;
    }
    if (speed_level > FAN_SPEED_HIGH) {
        return FAN_SPEED_HIGH;
    }
    return (fan_speed_t)speed_level;
}

static fan_speed_t get_effective_speed(void) {
    if (!g_status.fan_power_enabled) {
        return FAN_SPEED_OFF;
    }
    return sanitize_speed(g_status.fan_speed_enum);
}

static bool fan_is_running(void) {
    return get_effective_speed() != FAN_SPEED_OFF;
}

static void apply_fan_output(void) {
    fan_set_speed(get_effective_speed());
    gui_set_fan(fan_is_running());
}

// --- 核心逻辑 ---
static void push_status_indicator_update(void) {
    status_indicator_inputs_t inputs = {
        .wifi_connected = g_status.wifi_connected,
        .alert_high_pollution = g_status.alert_high_pollution,
        .alert_replace_filter = g_status.alert_replace_filter,
        .manual_mode = (g_status.mode == MODE_MANUAL),
        .night_mode = (g_status.mode == MODE_NIGHT),
        .fan_running = fan_is_running(),
    };

    status_indicator_sync(&inputs);
}

static void update_system_logic(void) {
    // 1. 警告
    bool pollution_warning = (g_status.pm25_val > 150) || (g_status.tvoc_val > 1000);
    g_status.alert_high_pollution = pollution_warning;
    bool filter_warning = (g_status.filter_life_hours > 300);
    g_status.alert_replace_filter = filter_warning;

    gui_set_warning(pollution_warning || filter_warning); // 异步发送到GUI

    // 2. 风速
    int target_speed = g_status.fan_speed_enum;

    if (g_status.mode == MODE_MANUAL) {
        // 保持当前设定 (由CMD_FAN直接修改)
    } else if (g_status.mode == MODE_NIGHT) {
        target_speed = 1; // LOW
    } else {
        // AUTO
        if (g_status.pm25_val <= 75) target_speed = 1;
        else if (g_status.pm25_val <= 115) target_speed = 2;
        else target_speed = 3;

        if (g_status.tvoc_val > 500 && target_speed < 2) target_speed = 2;
        if (g_status.eco2_val > 1000 && target_speed < 2) target_speed = 2;
        if (g_status.pm25_val > 150 || g_status.tvoc_val > 2000) target_speed = 3;
    }

    // 执行
    if (g_status.mode != MODE_MANUAL && target_speed != g_status.fan_speed_enum) {
        g_status.fan_speed_enum = target_speed;
    }

    apply_fan_output();
    push_status_indicator_update();
}


/**
 * @brief 控制中心线程函数
 */
static void control_thread_func(void *p1, void *p2, void *p3) {
    ctrl_msg_t msg;
    
    // Init Defaults
    gui_set_auto_mode(true);
    
    while(1) {
        if (k_msgq_get(&control_msgq, &msg, K_FOREVER) == 0) {
            switch(msg.type) {
                case CTRL_EVT_PM25:
                    g_status.pm25_val = msg.data.pm25;
                    gui_set_pm25((uint16_t)(g_status.pm25_val * 10));  // 更新GUI，乘10显示
                    break;
                case CTRL_EVT_ENV:
                    g_status.tvoc_val = msg.data.env.tvoc;
                    g_status.hcho_val = msg.data.env.hcho;
                    g_status.eco2_val = msg.data.env.eco2;
                    gui_set_env(g_status.tvoc_val, g_status.hcho_val, g_status.eco2_val);
                    break;
                case CTRL_EVT_TH:
                    g_status.temp_val = msg.data.th.temp;
                    g_status.hum_val = msg.data.th.hum;
                    gui_set_temp_hum((int16_t)g_status.temp_val, (uint16_t)g_status.hum_val);
                    break;
                case CTRL_EVT_WIFI:
                    gui_set_wifi(msg.data.wifi_connected);
                    g_status.wifi_connected = msg.data.wifi_connected;
                    break;
                case CTRL_EVT_CMD_MODE:
                    if (strcmp(msg.data.mode_str, "auto") == 0) {
                        g_status.mode = MODE_AUTO;
                        gui_set_auto_mode(true);
                    } else if (strcmp(msg.data.mode_str, "manual") == 0) {
                        g_status.mode = MODE_MANUAL;
                        gui_set_auto_mode(false);
                    } else if (strcmp(msg.data.mode_str, "night") == 0) {
                        g_status.mode = MODE_NIGHT;
                        gui_set_auto_mode(false);
                    }
                    printk("[Ctrl] Mode: %d\n", g_status.mode);
                    break;
                case CTRL_EVT_CMD_FAN:
                    if (g_status.mode == MODE_MANUAL) {
                        g_status.fan_speed_enum = msg.data.fan_speed_enum;
                        printk("[Ctrl] Manual Fan: %d\n", g_status.fan_speed_enum);
                    }
                    break;
                case CTRL_EVT_FAN_POWER_TOGGLE:
                    g_status.fan_power_enabled = !g_status.fan_power_enabled;
                    printk("[Ctrl] Fan power toggled -> %s\n", g_status.fan_power_enabled ? "ON" : "OFF");
                    break;
                default:
                    break;
            }
            // Run logic after every event
            update_system_logic();
        }
    }
}

K_THREAD_DEFINE(control_thread, 2048, control_thread_func, NULL, NULL, NULL, 5, 0, 0);

// --- APIs ---
void control_center_init(void) {
    // Thread defined by K_THREAD_DEFINE, starts auto.
}

/**
 * @brief 上报PM2.5数据
 * @param val PM2.5 数值，单位 ug/m3
 */
void control_report_pm25(uint32_t val) {
    ctrl_msg_t msg = { .type = CTRL_EVT_PM25, .data.pm25 = val };
    k_msgq_put(&control_msgq, &msg, K_NO_WAIT);
}

/**
 * @brief 上报环境数据
 * @param tvoc TVOC 数值，单位 ug/m3
 * @param hcho 甲醛数值，单位 ug/m3
 * @param eco2 eCO2 数值，单位 ppm
 */
void control_report_env(uint16_t tvoc, uint16_t hcho, uint16_t eco2) {
    ctrl_msg_t msg = { .type = CTRL_EVT_ENV, .data.env = {tvoc, hcho, eco2} };
    k_msgq_put(&control_msgq, &msg, K_NO_WAIT);
}

/**
 * @brief 上报温湿度数据
 * @param temp 温度，单位 摄氏度
 * @param hum 湿度，单位 百分比
 */
void control_report_temp_hum(float temp, float hum) {
    ctrl_msg_t msg = { .type = CTRL_EVT_TH, .data.th = {temp, hum} };
    k_msgq_put(&control_msgq, &msg, K_NO_WAIT);
}

/**
 * @brief 上报WiFi连接状态
 * @param connected 是否已连接
 */
void control_report_wifi_status(bool connected) {
    ctrl_msg_t msg = { .type = CTRL_EVT_WIFI, .data.wifi_connected = connected ? 1 : 0 };
    k_msgq_put(&control_msgq, &msg, K_NO_WAIT);
}

/**
 * @brief 设置系统模式
 * @param mode_str 模式字符串，"auto", "manual", "night"
 */
void control_set_mode(const char* mode_str) {
    ctrl_msg_t msg = { .type = CTRL_EVT_CMD_MODE };
    strncpy(msg.data.mode_str, mode_str, sizeof(msg.data.mode_str)-1);
    k_msgq_put(&control_msgq, &msg, K_NO_WAIT);
}

/**
 * @brief 设置风扇速度命令
 * @param speed_str 速度字符串，"off", "low", "medium", "high"
 */
void control_set_fan_cmd(const char* speed_str) {
    ctrl_msg_t msg = { .type = CTRL_EVT_CMD_FAN };
    int lvl = 0;
    if (strcmp(speed_str, "off") == 0) lvl = 0;
    else if (strcmp(speed_str, "low") == 0) lvl = 1;
    else if (strcmp(speed_str, "medium") == 0) lvl = 2;
    else if (strcmp(speed_str, "high") == 0) lvl = 3;
    msg.data.fan_speed_enum = lvl;
    k_msgq_put(&control_msgq, &msg, K_NO_WAIT);
}

void control_toggle_fan_power(void) {
    ctrl_msg_t msg = { .type = CTRL_EVT_FAN_POWER_TOGGLE };
    k_msgq_put(&control_msgq, &msg, K_NO_WAIT);
}

/**
 * @brief 获取当前系统状态
 * @param out_status 输出状态结构体指针
 */
void control_get_status(air_purifier_status_t *out_status) {
    if (out_status) memcpy(out_status, &g_status, sizeof(air_purifier_status_t));
}

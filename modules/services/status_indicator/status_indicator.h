#ifndef STATUS_INDICATOR_H_
#define STATUS_INDICATOR_H_

#include <stdbool.h>

/**
 * @brief 输入状态摘要，用于驱动 RGB 状态灯。
 */
typedef struct {
    bool wifi_connected;
    bool alert_high_pollution;
    bool alert_replace_filter;
    bool manual_mode;
    bool night_mode;
    bool fan_running;
} status_indicator_inputs_t;

/**
 * @brief RGB 指示灯的抽象状态，按照优先级在模块内部转换为颜色。
 */
typedef enum {
    STATUS_INDICATOR_STATE_BOOT = 0,
    STATUS_INDICATOR_STATE_WIFI_CONNECTING,
    STATUS_INDICATOR_STATE_NORMAL_ACTIVE,
    STATUS_INDICATOR_STATE_NORMAL_IDLE,
    STATUS_INDICATOR_STATE_MANUAL_MODE,
    STATUS_INDICATOR_STATE_NIGHT_MODE,
    STATUS_INDICATOR_STATE_ALERT_POLLUTION,
    STATUS_INDICATOR_STATE_ALERT_FILTER,
    STATUS_INDICATOR_STATE_COUNT,
} status_indicator_state_t;

void status_indicator_init(void);

void status_indicator_force_state(status_indicator_state_t state);

void status_indicator_sync(const status_indicator_inputs_t *inputs);

#endif /* STATUS_INDICATOR_H_ */

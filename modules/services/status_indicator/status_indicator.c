#include "status_indicator.h"
#include "rgb_led.h"

#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(status_indicator, LOG_LEVEL_INF);

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} rgb_color_t;

static bool indicator_ready = false;
static status_indicator_state_t current_state = STATUS_INDICATOR_STATE_BOOT;

static const rgb_color_t state_palette[STATUS_INDICATOR_STATE_COUNT] = {
    [STATUS_INDICATOR_STATE_BOOT] = {.r = 0, .g = 0, .b = 40}, // 上电：暗蓝
    [STATUS_INDICATOR_STATE_WIFI_CONNECTING] = {.r = 0, .g = 40, .b = 255}, // WiFi 配网：蓝色
    [STATUS_INDICATOR_STATE_NORMAL_ACTIVE] = {.r = 0, .g = 200, .b = 255}, // 自动运行：青色
    [STATUS_INDICATOR_STATE_NORMAL_IDLE] = {.r = 0, .g = 80, .b = 0}, // 待机：绿色
    [STATUS_INDICATOR_STATE_MANUAL_MODE] = {.r = 255, .g = 255, .b = 200}, // 手动：暖白
    [STATUS_INDICATOR_STATE_NIGHT_MODE] = {.r = 120, .g = 0, .b = 180}, // 夜间：紫色
    [STATUS_INDICATOR_STATE_ALERT_POLLUTION] = {.r = 255, .g = 0, .b = 0}, // 污染告警：红色
    [STATUS_INDICATOR_STATE_ALERT_FILTER] = {.r = 255, .g = 140, .b = 0}, // 滤芯告警：琥珀
};

BUILD_ASSERT(ARRAY_SIZE(state_palette) == STATUS_INDICATOR_STATE_COUNT, "palette mismatch");

static void apply_color(status_indicator_state_t state) {
    if (!indicator_ready) {
        return;
    }

    if (state >= STATUS_INDICATOR_STATE_COUNT) {
        return;
    }

    const rgb_color_t *color = &state_palette[state];
    rgb_led_set_color(color->r, color->g, color->b);
}

static status_indicator_state_t reduce_inputs(const status_indicator_inputs_t *inputs) {
    if (!inputs) {
        return STATUS_INDICATOR_STATE_BOOT;
    }

    if (inputs->alert_high_pollution) {
        return STATUS_INDICATOR_STATE_ALERT_POLLUTION;
    }

    if (inputs->alert_replace_filter) {
        return STATUS_INDICATOR_STATE_ALERT_FILTER;
    }

    if (!inputs->wifi_connected) {
        return STATUS_INDICATOR_STATE_WIFI_CONNECTING;
    }

    if (inputs->manual_mode) {
        return STATUS_INDICATOR_STATE_MANUAL_MODE;
    }

    if (inputs->night_mode) {
        return STATUS_INDICATOR_STATE_NIGHT_MODE;
    }

    if (inputs->fan_running) {
        return STATUS_INDICATOR_STATE_NORMAL_ACTIVE;
    }

    return STATUS_INDICATOR_STATE_NORMAL_IDLE;
}

void status_indicator_init(void) {
    int ret = rgb_led_init();
    if (ret != 0) {
        LOG_ERR("RGB LED init failed (%d)", ret);
        return;
    }

    indicator_ready = true;
    status_indicator_force_state(current_state);
}

void status_indicator_force_state(status_indicator_state_t state) {
    if (state >= STATUS_INDICATOR_STATE_COUNT) {
        return;
    }

    if (indicator_ready && state == current_state) {
        return;
    }

    current_state = state;
    apply_color(current_state);
}

void status_indicator_sync(const status_indicator_inputs_t *inputs) {
    status_indicator_state_t next_state = reduce_inputs(inputs);
    status_indicator_force_state(next_state);
}

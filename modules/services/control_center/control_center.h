#ifndef __CONTROL_CENTER_H__
#define __CONTROL_CENTER_H__

#include <zephyr/kernel.h>
#include <stdbool.h>

typedef enum {
    MODE_AUTO,  
    MODE_MANUAL,
    MODE_NIGHT
} system_mode_t;

typedef struct {
    // 传感器数据缓存
    uint32_t pm25_val;     // ug/m3
    uint16_t tvoc_val;     // ug/m3
    uint16_t hcho_val;     // ug/m3
    uint16_t eco2_val;     // ppm
    float    temp_val;     // C
    float    hum_val;      // %

    // 系统状态
    system_mode_t mode;
    int           fan_speed_enum; // 0=OFF, 1=LOW, 2=MED, 3=HIGH
    uint32_t      filter_life_hours; 
    
    // 警告标志
    bool alert_high_pollution;
    bool alert_replace_filter;
} air_purifier_status_t;


// --- API ---

// 启动控制中心线程
void control_center_init(void);

// 上报传感器数据
void control_report_pm25(uint32_t val);
void control_report_env(uint16_t tvoc, uint16_t hcho, uint16_t eco2);
void control_report_temp_hum(float temp, float hum);

// 下发控制指令
void control_set_mode(const char* mode_str);
void control_set_fan_cmd(const char* speed_str);

// 获取状态
void control_get_status(air_purifier_status_t *out_status);

#endif // __CONTROL_CENTER_H__
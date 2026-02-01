#ifndef DC01_APP_H
#define DC01_APP_H

#define SENSOR_CENTER_ENABLED 1 // 启用传感器中心模块(不启用则直接打印数据)

#if SENSOR_CENTER_ENABLED
#include "sensor_center.h"
#endif


/**
 * @brief 启动 DC01 传感器应用
 * @return 0 成功，-1 失败
 */
int dc01_sensor_app_start(void);

#endif /* DC01_APP_H */

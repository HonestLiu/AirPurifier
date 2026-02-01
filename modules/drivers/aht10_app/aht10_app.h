#ifndef __AHT10_APP_H__
#define __AHT10_APP_H__

#define SENSOR_CENTER_ENABLE 1 // 使用传感器中心模块(否则直接打印数据)

#if SENSOR_CENTER_ENABLE 
#include "sensor_center.h"
#endif

int aht10_app_start(void);

#endif // !__AHT10_APP_H__
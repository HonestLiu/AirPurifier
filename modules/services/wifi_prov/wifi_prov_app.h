#ifndef __WIFI_PROV_APP__
#define __WIFI_PROV_APP__

#include "wifi_prov_conf.h"

// 连接成功信号量
extern struct k_sem wifi_connected_sem;

int wifi_prov_app_start(void);

#endif // !__WIFI_PROV_APP__
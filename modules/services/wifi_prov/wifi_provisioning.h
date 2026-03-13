#ifndef WIFI_PROVISIONING_H
#define WIFI_PROVISIONING_H

#include <stdbool.h>
#include <zephyr/net/net_if.h>
#include "wifi_prov_conf.h"

int wifi_prov_init(struct net_if *ap, struct net_if *sta);

int wifi_prov_start(void);

void wifi_prov_stop(void);

void wifi_prov_on_sta_connected(void);

bool wifi_prov_has_creds(void);

const char *wifi_prov_get_ssid(void);

#endif

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/wifi_mgmt.h>      // Wi-Fi 管理 API（连接、AP 启用等）
#include <zephyr/net/dhcpv4_server.h> // DHCPv4 服务器 API
#include <zephyr/net/net_if.h>

#include "wifi_prov_app.h"
#include "wifi_provisioning.h"
#include "control_center.h"

// 注册日志模块，名称为 "WIFI_PROV_APP"
LOG_MODULE_REGISTER(WIFI_PROV_APP);

// 定义 MAC 地址格式化宏，用于打印形如 "AA:BB:CC:DD:EE:FF" 的地址
#define MACSTR "%02X:%02X:%02X:%02X:%02X:%02X"

// 定义需要监听的 Wi-Fi 网络事件掩码
#define NET_EVENT_WIFI_MASK                                                                        \
	(NET_EVENT_WIFI_CONNECT_RESULT |        /* STA 连接结果 */                                    \
	 NET_EVENT_WIFI_DISCONNECT_RESULT |     /* STA 断开结果 */                                    \
	 NET_EVENT_WIFI_AP_ENABLE_RESULT |      /* AP 启用结果 */                                     \
	 NET_EVENT_WIFI_AP_DISABLE_RESULT |     /* AP 禁用结果 */                                     \
	 NET_EVENT_WIFI_AP_STA_CONNECTED |      /* 有设备连接到 AP */                                 \
	 NET_EVENT_WIFI_AP_STA_DISCONNECTED)    /* 有设备从 AP 断开 */

// 全局变量：分别保存 AP 和 STA 模式的网络接口指针
static struct net_if *ap_iface;
static struct net_if *sta_iface;

// 全局变量：Wi-Fi 事件回调结构体
static struct net_mgmt_event_callback cb;

/* 编译时检查：确保关键配置项非空 */
BUILD_ASSERT(sizeof(CONFIG_WIFI_SAMPLE_AP_SSID) > 1,
	     "CONFIG_WIFI_SAMPLE_AP_SSID is empty. Please set it in conf file.");

/* STA SSID 可以为空，允许通过网页配网设置 */

// 如果启用了 DHCP 服务器，则还需检查 IP 和子网掩码配置
#if WIFI_SAMPLE_DHCPV4_START
BUILD_ASSERT(sizeof(CONFIG_WIFI_SAMPLE_AP_IP_ADDRESS) > 1,
	     "CONFIG_WIFI_SAMPLE_AP_IP_ADDRESS is empty. Please set it in conf file.");

BUILD_ASSERT(sizeof(CONFIG_WIFI_SAMPLE_AP_NETMASK) > 1,
	     "CONFIG_WIFI_SAMPLE_AP_NETMASK is empty. Please set it in conf file.");
#endif

K_SEM_DEFINE(wifi_connected_sem, 0, 1); // 连接成功信号量，初始值为 0，最大值为 1
int wifi_prov_app_stop(void);

/**
 * @brief Wi-Fi 事件处理回调函数
 * 
 * 当发生指定的 Wi-Fi 事件时，Zephyr 会调用此函数。
 * 
 * @param cb          回调结构体指针
 * @param mgmt_event  触发的事件类型
 * @param iface       相关的网络接口
 */
static void wifi_event_handler(struct net_mgmt_event_callback *cb, uint64_t mgmt_event,
			       struct net_if *iface)
{
	switch (mgmt_event) {
	case NET_EVENT_WIFI_CONNECT_RESULT: {
		LOG_INF("已成功连接到路由器: %s", wifi_prov_get_ssid());
		wifi_prov_on_sta_connected();
        wifi_prov_app_stop();
		control_report_wifi_status(true); // 上报连接状态
		k_sem_give(&wifi_connected_sem); // 释放连接成功信号量
		break;
	}
	case NET_EVENT_WIFI_DISCONNECT_RESULT: {
		LOG_INF("已从路由器断开: %s", wifi_prov_get_ssid());
        control_report_wifi_status(false); // 上报断开状态
		break;
	}
	case NET_EVENT_WIFI_AP_ENABLE_RESULT: {
		LOG_INF("AP 模式已启用，等待客户端设备连接...");
		break;
	}
	case NET_EVENT_WIFI_AP_DISABLE_RESULT: {
		LOG_INF("AP 模式已被禁用。");
		break;
	}
	case NET_EVENT_WIFI_AP_STA_CONNECTED: {
		// 从回调信息中提取连接设备的 MAC 地址
		struct wifi_ap_sta_info *sta_info = (struct wifi_ap_sta_info *)cb->info;
		LOG_INF("设备已连接热点: " MACSTR, 
			sta_info->mac[0], sta_info->mac[1], sta_info->mac[2],
			sta_info->mac[3], sta_info->mac[4], sta_info->mac[5]);
		break;
	}
	case NET_EVENT_WIFI_AP_STA_DISCONNECTED: {
		// 从回调信息中提取断开设备的 MAC 地址
		struct wifi_ap_sta_info *sta_info = (struct wifi_ap_sta_info *)cb->info;
		LOG_INF("设备已离开热点: " MACSTR,
			sta_info->mac[0], sta_info->mac[1], sta_info->mac[2],
			sta_info->mac[3], sta_info->mac[4], sta_info->mac[5]);
		break;
	}
	default:
		// 忽略其他未处理的事件
		break;
	}
}



int wifi_prov_app_start(void)
{
    // 等待 5 秒，避免日志被早期启动信息冲掉
	k_sleep(K_SECONDS(5));

	// 初始化并注册事件回调
	net_mgmt_init_event_callback(&cb, wifi_event_handler, NET_EVENT_WIFI_MASK); // 设置回调和事件掩码
	net_mgmt_add_event_callback(&cb);	// 注册回调

	// 获取 AP 模式的网络接口（SoftAP）
	ap_iface = net_if_get_wifi_sap();

	// 获取 STA 模式的网络接口（Station）
	sta_iface = net_if_get_wifi_sta();

	wifi_prov_init(ap_iface, sta_iface); // 初始化 Wi-Fi 配网模块，传入 AP 和 STA 接口
	wifi_prov_start(); // 启动 Wi-Fi 配网模块

    return 0;
}

int wifi_prov_app_stop(void)
{
	wifi_prov_stop();

#if CONFIG_WIFI_SAMPLE_DHCPV4_START
	net_dhcpv4_server_stop(ap_iface);
	LOG_INF("DHCPv4 服务器已停止。");
#endif

	net_mgmt_del_event_callback(&cb);

	return 0;
}

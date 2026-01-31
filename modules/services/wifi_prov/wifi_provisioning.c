#include "wifi_provisioning.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/dhcpv4_server.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/socket.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/fs/nvs.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/atomic.h>

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

LOG_MODULE_REGISTER(WIFI_PROV);

// Captive Portal 配置
#define HTTP_PORT 80
#define DNS_PORT 53
#define HTTP_BUF_SIZE 1024
#define DNS_BUF_SIZE 512

static struct net_if *ap_iface;
static struct net_if *sta_iface;

static struct wifi_connect_req_params ap_config;
static struct wifi_connect_req_params sta_config;

static struct nvs_fs nvs;

static char sta_ssid[33];
static char sta_psk[65];
static bool sta_has_creds;

/* 改为指针，用于动态分配栈 */
static k_thread_stack_t *http_stack = NULL;
static k_thread_stack_t *dns_stack = NULL;
static struct k_thread http_thread;
static struct k_thread dns_thread;

static int http_server_fd = -1;
static int dns_server_fd = -1;
static atomic_t portal_running = ATOMIC_INIT(0); /* Default to 0, start on demand */
static struct k_work portal_stop_work;
static struct k_work_delayable ap_fallback_work; /* Work for delayed AP start */

static void disable_ap_mode(void);
static void start_ap_services(void);       /* Forward declaration */

static void ap_fallback_work_handler(struct k_work *work)
{
    LOG_WRN("Wi-Fi 连接超时，启动 AP 配网模式...");
    start_ap_services();
}

static void stop_captive_portal(void)
{
	if (!atomic_cas(&portal_running, 1, 0)) {
		return;
	}

	if (http_server_fd >= 0) {
		zsock_close(http_server_fd);
		http_server_fd = -1;
	}

	if (dns_server_fd >= 0) {
		zsock_close(dns_server_fd);
		dns_server_fd = -1;
	}
}

static void portal_stop_work_handler(struct k_work *work)
{
	ARG_UNUSED(work);
	stop_captive_portal();
	
	/* 等待线程结束并释放栈内存 */
	if (http_stack) {
		k_thread_join(&http_thread, K_FOREVER);
		k_free(http_stack);
		http_stack = NULL;
	}
	if (dns_stack) {
		k_thread_join(&dns_thread, K_FOREVER);
		k_free(dns_stack);
		dns_stack = NULL;
	}

	disable_ap_mode();
}

static int nvs_init(void)
{
	const struct flash_area *fa;
	int ret = flash_area_open(FIXED_PARTITION_ID(storage_partition), &fa);
	if (ret) {
		LOG_ERR("NVS: 打开存储分区失败: %d", ret);
		return ret;
	}

	nvs.flash_device = flash_area_get_device(fa);
	nvs.offset = fa->fa_off;

	struct flash_pages_info info;
	ret = flash_get_page_info_by_offs(nvs.flash_device, nvs.offset, &info);
	if (ret) {
		LOG_ERR("NVS: 获取页信息失败: %d", ret);
		flash_area_close(fa);
		return ret;
	}

	nvs.sector_size = info.size;
	nvs.sector_count = fa->fa_size / info.size;
	flash_area_close(fa);

	ret = nvs_mount(&nvs);
	if (ret) {
		LOG_ERR("NVS: 挂载失败: %d", ret);
		return ret;
	}

	return 0;
}

/**
 * @brief 从 NVS 加载已保存的 STA 配网信息
 */
static void load_sta_creds_from_nvs(void)
{
	int len = nvs_read(&nvs, 1, sta_ssid, sizeof(sta_ssid));
	if (len > 0 && sta_ssid[0] != '\0') {
		sta_has_creds = true;
	}

	len = nvs_read(&nvs, 2, sta_psk, sizeof(sta_psk));
	if (len <= 0) {
		sta_psk[0] = '\0';
	}
}

/**
 * @brief 将 STA 配网信息保存到 NVS
 */
static void save_sta_creds_to_nvs(const char *ssid, const char *psk)
{
	if (!ssid || ssid[0] == '\0') {
		return;
	}

	nvs_write(&nvs, 1, ssid, strlen(ssid) + 1);
	nvs_write(&nvs, 2, psk, strlen(psk) + 1);
}

static void set_sta_creds(const char *ssid, const char *psk)
{
	strncpy(sta_ssid, ssid, sizeof(sta_ssid) - 1);
	sta_ssid[sizeof(sta_ssid) - 1] = '\0';
	if (psk) {
		strncpy(sta_psk, psk, sizeof(sta_psk) - 1);
		sta_psk[sizeof(sta_psk) - 1] = '\0';
	} else {
		sta_psk[0] = '\0';
	}
	sta_has_creds = true;
}

/**
 * @brief 连接到已保存的 Wi-Fi 网络
 */
static int connect_to_wifi(void)
{
	if (!sta_iface) {
		LOG_INF("STA 接口未初始化");
		return -EIO;
	}

	if (!sta_has_creds) {
		LOG_INF("未找到 STA 配网信息，等待网页配网...");
		return 0;
	}

	sta_config.ssid = (const uint8_t *)sta_ssid;
	sta_config.ssid_length = strlen(sta_ssid);
	sta_config.psk = (const uint8_t *)sta_psk;
	sta_config.psk_length = strlen(sta_psk);

	if (sta_config.psk_length == 0) {
		sta_config.security = WIFI_SECURITY_TYPE_NONE;
	} else {
		sta_config.security = WIFI_SECURITY_TYPE_PSK;
	}
	sta_config.channel = WIFI_CHANNEL_ANY;
	sta_config.band = WIFI_FREQ_BAND_2_4_GHZ;

	LOG_INF("正在连接到路由器: %s", sta_config.ssid);

	int ret = net_mgmt(NET_REQUEST_WIFI_CONNECT, sta_iface, &sta_config,
			   sizeof(struct wifi_connect_req_params));
	if (ret) {
		LOG_ERR("无法连接到路由器 (%s)", sta_ssid);
	}

	return ret;
}

static void url_decode(char *dst, const char *src, size_t dst_len)
{
	size_t di = 0;
	for (size_t si = 0; src[si] != '\0' && di + 1 < dst_len; si++) {
		if (src[si] == '+') {
			dst[di++] = ' ';
		} else if (src[si] == '%' && isxdigit((unsigned char)src[si + 1]) &&
			   isxdigit((unsigned char)src[si + 2])) {
			char hex[3] = { src[si + 1], src[si + 2], '\0' };
			dst[di++] = (char)strtol(hex, NULL, 16);
			si += 2;
		} else {
			dst[di++] = src[si];
		}
	}
	dst[di] = '\0';
}

static void parse_form_body(const char *body, char *ssid_out, size_t ssid_len,
			   char *psk_out, size_t psk_len)
{
	ssid_out[0] = '\0';
	psk_out[0] = '\0';

	const char *ssid_key = strstr(body, "ssid=");
	if (ssid_key) {
		ssid_key += 5;
		const char *end = strchr(ssid_key, '&');
		char temp[128] = {0};
		if (end) {
			size_t len = MIN((size_t)(end - ssid_key), sizeof(temp) - 1);
			memcpy(temp, ssid_key, len);
		} else {
			strncpy(temp, ssid_key, sizeof(temp) - 1);
		}
		url_decode(ssid_out, temp, ssid_len);
	}

	const char *psk_key = strstr(body, "psk=");
	if (psk_key) {
		psk_key += 4;
		const char *end = strchr(psk_key, '&');
		char temp[128] = {0};
		if (end) {
			size_t len = MIN((size_t)(end - psk_key), sizeof(temp) - 1);
			memcpy(temp, psk_key, len);
		} else {
			strncpy(temp, psk_key, sizeof(temp) - 1);
		}
		url_decode(psk_out, temp, psk_len);
	}
}

static void http_send_response(int client, const char *status,
			      const char *content_type, const char *body)
{
	char header[256];
	int len = snprintf(header, sizeof(header),
			   "HTTP/1.1 %s\r\n"
			   "Content-Type: %s\r\n"
			   "Content-Length: %zu\r\n"
			   "Connection: close\r\n\r\n",
			   status, content_type, strlen(body));
	zsock_send(client, header, len, 0);
	zsock_send(client, body, strlen(body), 0);
}

static void http_server_thread(void *a, void *b, void *c)
{
	ARG_UNUSED(a);
	ARG_UNUSED(b);
	ARG_UNUSED(c);

	// 创建 TCP 服务器套接字
	int server = zsock_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (server < 0) {
		LOG_ERR("HTTP: socket 失败");
		return;
	}
	http_server_fd = server;

	// 绑定到指定端口
	struct sockaddr_in addr = {0};
	addr.sin_family = AF_INET;
	addr.sin_port = htons(HTTP_PORT);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);

	// 监听连接
	if (zsock_bind(server, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		LOG_ERR("HTTP: bind 失败");
		zsock_close(server);
		return;
	}

	// 监听套接字
	if (zsock_listen(server, 4) < 0) {
		LOG_ERR("HTTP: listen 失败");
		zsock_close(server);
		return;
	}

	LOG_INF("HTTP: Captive Portal 已启动");

	while (atomic_get(&portal_running)) {
		// 接受客户端连接
		int client = zsock_accept(server, NULL, NULL);
		if (client < 0) {
			if (!atomic_get(&portal_running)) {
				break;
			}
			continue;
		}
		// 读取请求数据
		char buf[HTTP_BUF_SIZE] = {0};
		int r = zsock_recv(client, buf, sizeof(buf) - 1, 0);
		if (r <= 0) {
			zsock_close(client);
			continue;
		}

		bool is_post = (strncmp(buf, "POST ", 5) == 0);
		char *body = strstr(buf, "\r\n\r\n");
		if (body) {
			body += 4;
		}

		if (is_post && body) {
			char ssid[33];
			char psk[65];
			parse_form_body(body, ssid, sizeof(ssid), psk, sizeof(psk));
			if (ssid[0] != '\0') {
				set_sta_creds(ssid, psk);
				save_sta_creds_to_nvs(ssid, psk);
				connect_to_wifi();
				http_send_response(client, "200 OK", "text/html",
					"<html><body><h2>Configuration Saved</h2><p>The device is connecting to the router...</p></body></html>");
			} else {
				http_send_response(client, "400 Bad Request", "text/html",
					"<html><body><h2>SSID cannot be empty</h2></body></html>");
			}
		} else {
			http_send_response(client, "200 OK", "text/html",
				"<html><body><h2>Wi-Fi Configuration</h2>"
				"<form method=\"POST\" action=\"/configure\">"
				"SSID: <input name=\"ssid\" /><br/>"
				"PSK: <input name=\"psk\" type=\"password\" /><br/>"
				"<button type=\"submit\">Save</button>"
				"</form></body></html>");
		}

		zsock_close(client);
	}

	if (server >= 0) {
		zsock_close(server);
	}
}

/**
 * @brief 简单的 DNS 服务器线程，响应所有查询为 AP 的 IP 地址
 */
static void dns_server_thread(void *a, void *b, void *c)
{
	ARG_UNUSED(a);
	ARG_UNUSED(b);
	ARG_UNUSED(c);

	int sock = zsock_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sock < 0) {
		LOG_ERR("DNS: socket 失败");
		return;
	}
	dns_server_fd = sock;

	struct sockaddr_in addr = {0};
	addr.sin_family = AF_INET;
	addr.sin_port = htons(DNS_PORT);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);

	if (zsock_bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		LOG_ERR("DNS: bind 失败");
		zsock_close(sock);
		return;
	}

	LOG_INF("DNS: Captive Portal DNS 已启动");

	while (atomic_get(&portal_running)) {
		uint8_t buf[DNS_BUF_SIZE];
		struct sockaddr_in client;
		socklen_t len = sizeof(client);
		int r = zsock_recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr *)&client, &len);
		if (r < 12) {
			if (!atomic_get(&portal_running)) {
				break;
			}
			continue;
		}

		uint16_t id = (buf[0] << 8) | buf[1];
		uint8_t response[DNS_BUF_SIZE];
		memset(response, 0, sizeof(response));
		response[0] = (id >> 8) & 0xFF;
		response[1] = id & 0xFF;
		response[2] = 0x81;
		response[3] = 0x80;
		response[4] = 0x00;
		response[5] = 0x01;
		response[6] = 0x00;
		response[7] = 0x01;

		int qlen = 12;
		while (qlen < r && buf[qlen] != 0) {
			qlen += buf[qlen] + 1;
		}
		qlen += 5;
		if (qlen > r || qlen + 16 > DNS_BUF_SIZE) {
			continue;
		}

		memcpy(&response[12], &buf[12], qlen - 12);

		int pos = qlen;
		response[pos++] = 0xC0;
		response[pos++] = 0x0C;
		response[pos++] = 0x00;
		response[pos++] = 0x01;
		response[pos++] = 0x00;
		response[pos++] = 0x01;
		response[pos++] = 0x00;
		response[pos++] = 0x00;
		response[pos++] = 0x00;
		response[pos++] = 0x3C;
		response[pos++] = 0x00;
		response[pos++] = 0x04;

		struct in_addr ap_addr;
		net_addr_pton(AF_INET, CONFIG_WIFI_SAMPLE_AP_IP_ADDRESS, &ap_addr);
		memcpy(&response[pos], &ap_addr.s_addr, 4);
		pos += 4;

		zsock_sendto(sock, response, pos, 0, (struct sockaddr *)&client, len);
	}

	if (sock >= 0) {
		zsock_close(sock);
	}
}

/**
 * @brief 启用 AP 模式（开启热点）
 */
static void enable_ap_mode(void)
{
	if (!ap_iface) {
		LOG_INF("AP 接口未初始化");
		return;
	}

	LOG_INF("正在开启 AP 模式（热点）");

	ap_config.ssid = (const uint8_t *)CONFIG_WIFI_SAMPLE_AP_SSID;
	ap_config.ssid_length = sizeof(CONFIG_WIFI_SAMPLE_AP_SSID) - 1;

	ap_config.psk = (const uint8_t *)CONFIG_WIFI_SAMPLE_AP_PSK;
	ap_config.psk_length = sizeof(CONFIG_WIFI_SAMPLE_AP_PSK) - 1;

	ap_config.channel = WIFI_CHANNEL_ANY;
	ap_config.band = WIFI_FREQ_BAND_2_4_GHZ;

	if (sizeof(CONFIG_WIFI_SAMPLE_AP_PSK) == 1) {
		ap_config.security = WIFI_SECURITY_TYPE_NONE;
	} else {
		ap_config.security = WIFI_SECURITY_TYPE_PSK;
	}

#if CONFIG_WIFI_SAMPLE_DHCPV4_START
	static struct net_in_addr addr;
	static struct net_in_addr netmaskAddr;

	// 配置 AP 接口的静态 IP 地址
	if (net_addr_pton(NET_AF_INET, CONFIG_WIFI_SAMPLE_AP_IP_ADDRESS, &addr)) {
		LOG_ERR("无效的 AP IP 地址: %s", CONFIG_WIFI_SAMPLE_AP_IP_ADDRESS);
		return;
	}

	// 配置子网掩码
	if (net_addr_pton(NET_AF_INET, CONFIG_WIFI_SAMPLE_AP_NETMASK, &netmaskAddr)) {
		LOG_ERR("无效的子网掩码: %s", CONFIG_WIFI_SAMPLE_AP_NETMASK);
		return;
	}

	// 设置网关和 IP 地址
	net_if_ipv4_set_gw(ap_iface, &addr);
	if (net_if_ipv4_addr_add(ap_iface, &addr, NET_ADDR_MANUAL, 0) == NULL) {
		LOG_ERR("无法为 AP 接口设置 IP 地址");
	}

	// 设置子网掩码
	if (!net_if_ipv4_set_netmask_by_addr(ap_iface, &addr, &netmaskAddr)) {
		LOG_ERR("无法为 AP 接口设置子网掩码: %s", CONFIG_WIFI_SAMPLE_AP_NETMASK);
	}

	// 启动 DHCPv4 服务器
	addr.s4_addr[3] += 10;
	if (net_dhcpv4_server_start(ap_iface, &addr) != 0) {
		LOG_ERR("DHCP 服务器启动失败");
		return;
	}

	LOG_INF("DHCPv4 服务器已启动...");
#endif

	// 启用 AP 模式
	int ret = net_mgmt(NET_REQUEST_WIFI_AP_ENABLE, ap_iface, &ap_config,
			   sizeof(struct wifi_connect_req_params));
	if (ret) {
		LOG_ERR("启用 AP 模式失败，错误码: %d", ret);
	}
}

static void disable_ap_mode(void)
{
	if (!ap_iface) {
		return;
	}

	//if (net_dhcpv4_server_stop(ap_iface) != 0) {
	//	LOG_WRN("DHCP 服务器停止失败");
	//}

	int ret = net_mgmt(NET_REQUEST_WIFI_AP_DISABLE, ap_iface, NULL, 0);
	if (ret) {
		LOG_WRN("禁用 AP 模式失败: %d", ret);
	}
}

/**
 * @brief 初始化 Wi-Fi 配网模块
 * @param ap AP 模式使用的网络接口
 * @param sta STA 模式使用的网络接口
 * @return 0 成功；负值表示错误
 */
int wifi_prov_init(struct net_if *ap, struct net_if *sta)
{
	ap_iface = ap;
	sta_iface = sta;
	// 初始化停止 Captive Portal 的工作项
	k_work_init(&portal_stop_work, portal_stop_work_handler);
	// 初始化 AP 回退工作项（超时未连接则启动 AP）
	k_work_init_delayable(&ap_fallback_work, ap_fallback_work_handler);

	int ret = nvs_init();
	if (ret == 0) {
		// 从 NVS 加载已保存的 STA 配网信息
		load_sta_creds_from_nvs();
	}

	if (!sta_has_creds) {
		LOG_INF("未找到已保存的 STA 配网信息，进入网页配网模式");
	}

	return ret;
}

/**
 * @brief 启动 Wi-Fi 配网模块（开启 Captive Portal）
 * @return 0 成功；负值表示错误
 */
static void start_ap_services(void)
{
	if (atomic_get(&portal_running)) {
		return;
	}

	LOG_INF("启动配网服务 (AP + HTTP + DNS)...");

	// 启动 Captive Portal
	atomic_set(&portal_running, 1);
	// 启用 AP 模式（开启热点）
	enable_ap_mode();

	/* 动态分配栈空间 */
	/* K_THREAD_STACK_ALIGN not publicly exposed, use ARCH_STACK_PTR_ALIGN or safe default */
	#ifndef K_THREAD_STACK_ALIGN
	#define K_THREAD_STACK_ALIGN ARCH_STACK_PTR_ALIGN
	#endif

	if (!dns_stack) {
		dns_stack = k_aligned_alloc(K_THREAD_STACK_ALIGN, K_THREAD_STACK_LEN(2048));
	}
	if (!http_stack) {
		http_stack = k_aligned_alloc(K_THREAD_STACK_ALIGN, K_THREAD_STACK_LEN(4096));
	}

	if (dns_stack && http_stack) {
		// 启动 DNS 和 HTTP 服务器线程
		k_thread_create(&dns_thread, dns_stack, K_THREAD_STACK_LEN(2048),
				dns_server_thread, NULL, NULL, NULL, 5, 0, K_NO_WAIT);
		k_thread_create(&http_thread, http_stack, K_THREAD_STACK_LEN(4096),
				http_server_thread, NULL, NULL, NULL, 5, 0, K_NO_WAIT);
	} else {
		LOG_ERR("无法为配网线程分配栈空间");
	}
}

/**
 * @brief 启动 Wi-Fi 配网模块（开启 Captive Portal）
 * @return 0 成功；负值表示错误
 */
int wifi_prov_start(void)
{
	/* 优化逻辑：如果有已保存的 Wi-Fi 信息，先尝试连接，超时再启动 AP */
	if (sta_has_creds) {
		LOG_INF("检测到已保存的配网信息，尝试连接 Wi-Fi (超时 20s)...");
		
		/* 启动 20秒 的超时回退计时器 */
		k_work_schedule(&ap_fallback_work, K_SECONDS(20));
		
		/* 尝试连接 */
		return connect_to_wifi();
	} 
	
	/* 没有配置信息，直接启动 AP */
	start_ap_services();
	return 0;
}

void wifi_prov_stop(void)
{
	k_work_submit(&portal_stop_work);
}

void wifi_prov_on_sta_connected(void)
{
	/* 成功连接：取消 AP 回退计时器 */
	k_work_cancel_delayable(&ap_fallback_work);
	
	/* 停止配网服务（如果正在运行） */
	k_work_submit(&portal_stop_work);
}

bool wifi_prov_has_creds(void)
{
	return sta_has_creds;
}

const char *wifi_prov_get_ssid(void)
{
	return sta_ssid;
}

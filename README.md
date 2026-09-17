# Air Purifier 智能空气净化器固件

基于 **Zephyr RTOS** 的智能空气净化器控制终端固件，运行在 STM32F407VGT6 与 ESP32-S3 两款 MCU 上。
集成了 OLED 图形界面、多传感器数据采集、MQTT 云联、风扇 PWM 控制、RGB 状态指示灯与实体按键交互，是一套完整的空气净化器嵌入式软件方案。

---

## 目录

- [功能特性](#功能特性)
- [硬件平台](#硬件平台)
- [软件架构](#软件架构)
- [模块说明](#模块说明)
  - [services 服务层](#services-服务层)
  - [drivers 驱动层](#drivers-驱动层)
  - [display 显示层](#display-显示层)
- [目录结构](#目录结构)
- [编译与烧录](#编译与烧录)
- [使用说明](#使用说明)
- [MQTT 协议](#mqtt-协议)
- [工程历史](#工程历史)

---

## 功能特性

- **OLED 图形界面（GUI）**：基于 U8G2 库驱动 SSD1306（128×64）OLED，界面由 lopaka.app 在线工具设计后移植，实现传感器数据、工作模式、Wi-Fi 状态等可视化显示。
- **多传感器采集**：通过 UART 接入 DC01（PM2.5 激光粉尘传感器）与 TOVC-301（VOC/甲醛/eCO₂ 传感器），通过 I2C 接入 AHT10（温湿度传感器），并通过**传感器中心**统一汇聚。
- **Wi-Fi 自动配网**：ESP32-S3 自带 AP（`ESP32-AP`），首次上电进入配网模式，用户连接 AP 后通过网页提交 Wi-Fi 凭据，凭据存入 NVS 持久化，重启后自动连接。
- **MQTT 云联**：连接 MQTT broker，周期性上报传感器数据；订阅控制主题接收云端下发的开关、风速、模式等指令。
- **风扇 PWM 调速**：通过 LEDC PWM 通道控制风扇，支持 关闭/低/中/高 四档；并配有**实体起停按钮**。
- **RGB 状态指示灯**：三路 PWM 驱动 RGB LED，按系统运行状态（开机、配网中、正常运行、手动/夜间模式、污染超标、滤芯更换提醒等）显示不同颜色。
- **控制中心**：集中管理系统模式（自动 / 手动 / 夜间）、风速、滤芯寿命，以及污染超标 / 滤芯更换等告警逻辑。

---

## 硬件平台

| 平台 | 芯片 | board 名称 | overlay 文件 |
| --- | --- | --- | --- |
| STM32 开发板 | STM32F407VGT6 | `stm32f4_disco` | `boards/stm32f4_disco.overlay` |
| ESP32-S3 开发板 | ESP32-S3 | `esp32s3_devkitc/esp32s3/procpu` | `boards/esp32s3_devkitc.overlay` |

> 备注：ESP32-S3 具备 Wi-Fi 能力，为功能完整的运行目标；STM32F407 仅配置了 OLED 显示别名（最小验证平台）。

**外设硬件清单**

| 外设 | 型号 / 类型 | 接口 | 说明 |
| --- | --- | --- | --- |
| OLED 显示屏 | SSD1306 128×64 | I2C | 由 U8G2 库直接驱动（ESP32-S3：GPIO1=SCL，GPIO2=SDA） |
| 激光粉尘传感器 | DC01 | UART | PM2.5 浓度监测 |
| 空气质量传感器 | TOVC-301 | UART | TVOC（µg/m³）/ 甲醛（µg/m³）/ eCO₂（ppm） |
| 温湿度传感器 | AHT10 | I2C | 温度（℃）/ 相对湿度（%） |
| 风扇 | PWM（LEDC） | PWM | 55556ns 周期，4 档调速 |
| RGB 状态灯 | 三路 PWM LED | PWM | 红 / 绿 / 蓝 独立 PWM 通道 |
| 实体按键 | 按钮 | GPIO | 物理风扇起停 |

---

## 软件架构

整体架构为 **模块化分层**，各模块通过 `CMakeLists.txt` 显式加入编译：

```
┌─────────────────────────────────────────────────────────┐
│                       src/main.c  (启动入口)              │
└───────────┬──────────────────────────────┬──────────────┘
            │ 创建线程                        │ 依序启动各服务
   ┌────────▼────────┐              ┌───────▼──────────────────┐
   │  GUI 线程        │              │ Wi-Fi 配网 → MQTT        │
   │  (gui_thread)    │              │ DC01/TOVC/AHT10 传感器   │
   └────────┬────────┘              │ 风扇 / RGB灯 / 按钮      │
            │                       └───────┬──────────────────┘
   ┌────────▼───────────────────────────────▼───────────────┐
   │              control_center (控制中心 / 业务逻辑)         │
   └───────┬──────────────────────────┬─────────────────────┘
           │                          │
   ┌───────▼────────┐         ┌───────▼────────────┐
   │  sensor_center  │         │  status_indicator  │
   │  (传感器消息队列) │         │  (状态指示灯服务)     │
   └───────┬────────┘         └────────────────────┘
           │  上报
   ┌───────▼──────────────────────────┐
   │  dc01 / tovc_301(公)  aht10(公)   │
   └──────────────────────────────────┘
```

### 线程与启动顺序

`main()` 首先创建两个高优先级线程：

1. **GUI 线程**（优先级 5）：负责显示刷新，确保上电后屏幕立即响应。
2. **传感器中心线程**（优先级 6）：`sensor_hub_thread`，从消息队列消费传感器数据。

随后依序初始化各服务：Wi-Fi 配网（阻塞等待连接成功，超时 1 小时自动重启）→ MQTT → DC01 → TOVC-301 → AHT10 → 风扇 → 状态指示灯 → 按钮服务。

---

## 模块说明

### services 服务层

| 模块 | 路径 | 职责 |
| --- | --- | --- |
| **control_center**（控制中心） | `modules/services/control_center/` | 系统核心业务。维护 `air_purifier_status_t` 状态（PM2.5/TVOC/甲醛/eCO₂/温湿度、模式、风速、滤芯寿命、告警标志、Wi-Fi 状态）。提供 `control_report_*` 上报与 `control_set_*` / `control_toggle_fan_power` 控制接口。模式枚举：`MODE_AUTO` / `MODE_MANUAL` / `MODE_NIGHT`；风速枚举：`0=OFF, 1=LOW, 2=MED, 3=HIGH`。 |
| **sensor_center**（传感器中心） | `modules/services/sensor_center/` | 统一的数据汇聚通道。定义 `struct sensor_event`（union 承载 DC01 / TOVC-301 / AHT10 三类数据，数值放大以保留精度），通过消息队列 `sensor_hub_queue` 异步分发。 |
| **wifi_prov**（Wi-Fi 配网） | `modules/services/wifi_prov/` | 软 AP + 网页配网。AP SSID 默认 `ESP32-AP`（`192.168.4.1`），配网资源动态释放；凭据存入 NVS，连接成功后释放 `wifi_connected_sem` 信号量以唤醒主流程。可通过 `wifi_prov_conf.h` 修改 SSID/密码。 |
| **mqtt**（MQTT 业务） | `modules/services/mqtt/` | broker 连接与业务封装。JSON 编解码、控制指令解析。 |
| **status_indicator**（状态指示灯） | `modules/services/status_indicator/` | 将系统状态摘要 `status_indicator_inputs_t` 按优先级映射为 RGB 颜色方案，驱动状态灯。 |

### drivers 驱动层

| 模块 | 路径 | 接口 |
| --- | --- | --- |
| **DC01**（PM2.5） | `modules/drivers/dc01/` | UART 驱动，`dc01_sensor_app_start()` |
| **TOVC-301**（空气质量） | `modules/drivers/tovc_301/` | UART 驱动，`tovc_sensor_app_start()` |
| **AHT10**（温湿度） | `modules/drivers/aht10_app/` | 封装外部 Zephyr AHT10 驱动为应用层，`aht10_app_start()` |
| **fan**（风扇） | `modules/drivers/fan/` | PWM 控制，`fan_set_speed(FAN_SPEED_OFF/LOW/MEDIUM/HIGH)` |
| **rgb_led**（RGB 灯） | `modules/drivers/rgb_led/` | 三路 PWM，`rgb_led_set_color(r, g, b)` |
| **button_app**（按键） | `modules/drivers/button_app/` | 实体按键扫描，`button_app_start()` |

> DC01 / TOVC-301 / AHT10 均通过 `SENSOR_CENTER_ENABLE` 开关选择数据去向：置 1 上报传感器中心，置 0 则直接打印。

### display 显示层

| 模块 | 路径 | 说明 |
| --- | --- | --- |
| **U8G2** | `modules/display/u8g2/` | 图形库源码（`csrc/`），按需裁剪编译 |
| **Zephyr 移植层** | `modules/display/u8g2_zephyr_port/` | 适配 Zephyr I2C 平台的字节/GPIO 回调 |
| **GUI** | `modules/display/gui/` | `gui_app.c` 线程 + `gui.c` 绘制逻辑 |

---

## 目录结构

```
AirPurifier/
├── CMakeLists.txt              # 顶层构建配置（模块引入、头文件、符号）
├── prj.conf                    # Zephyr 内核 / 网络 / MQTT / 传感器配置
├── boards/
│   ├── stm32f4_disco.overlay   # STM32F407 设备树覆盖
│   └── esp32s3_devkitc.overlay # ESP32-S3 设备树覆盖（外设别名、PWM、Wi-Fi）
├── src/
│   └── main.c                  # 主入口：线程创建 + 各服务启动
└── modules/
    ├── display/
    │   ├── gui/                # GUI 线程与绘制
    │   ├── u8g2/               # U8G2 图形库源码
    │   └── u8g2_zephyr_port/   # U8G2 在 Zephyr 上的移植
    ├── services/
    │   ├── control_center/     # 控制中心（业务逻辑）
    │   ├── sensor_center/      # 传感器数据中心
    │   ├── wifi_prov/          # Wi-Fi 自动配网
    │   ├── mqtt/               # MQTT 业务封装
    │   └── status_indicator/   # 状态指示灯服务
    └── drivers/
        ├── dc01/               # DC01 PM2.5 传感器驱动
        ├── tovc_301/           # TOVC-301 空气质量驱动
        ├── aht10_app/          # AHT10 温湿度应用层
        ├── fan/                # 风扇 PWM 驱动
        ├── rgb_led/            # RGB PWM 灯驱动
        └── button_app/         # 实体按键服务
```

---

## 编译与烧录

项目基于 Zephyr RTOS，需要先安装并配置 [Zephyr SDK](https://docs.zephyrproject.org/latest/develop/getting_started/index.html) 环境（`west` 工具链）。

```shell
# 1. 环境变量（已安装 Zephyr 时）
export ZEPHYR_BASE=/path/to/zephyr

# 2. STM32F407VGT6
west build -p always -b stm32f4_disco . \
    -DDTC_OVERLAY_FILE=boards/stm32f4_disco.overlay

# 3. ESP32-S3
west build -p always -b esp32s3_devkitc/esp32s3/procpu . \
    -DDTC_OVERLAY_FILE=boards/esp32s3_devkitc.overlay

# 4. 烧录（ESP32-S3，串口按实际修改）
python -m esptool --port "COMxx" --chip auto --baud 921600 \
    --before default-reset --after hard_reset write_flash -u \
    --flash-size detect 0x0 ./build/zephyr/zephyr.bin
```

### 关键配置（prj.conf）

| 分类 | 关键项 | 说明 |
| --- | --- | --- |
| 网络 | `CONFIG_WIFI=y`、`CONFIG_WIFI_NM=y` | 启用 Wi-Fi |
| | `CONFIG_WIFI_USAGE_MODE_STA_AP=y` | 同时支持 STA（联网）与 AP（配网） |
| | `CONFIG_NET_DHCPV4=y`、`CONFIG_NET_DHCPV4_SERVER=y` | 作为客户端联网 + 作为 AP 提供 DHCP |
| 存储 | `CONFIG_NVS=y`、`CONFIG_FLASH=y` | Wi-Fi 凭据持久化 |
| MQTT | `CONFIG_MQTT_LIB=y`、`CONFIG_JSON_LIBRARY=y` | MQTT 库与 JSON 解析 |
| 外设 | `CONFIG_SENSOR=y`、`CONFIG_AHT10=y`、`CONFIG_PWM=y`、`CONFIG_GPIO=y`、`CONFIG_I2C=y` | 传感器 / PWM / GPIO / I2C |
| | `CONFIG_UART_INTERRUPT_DRIVEN=y` | UART 传感器中断驱动 |
| 资源 | `CONFIG_HEAP_MEM_POOL_SIZE=98304` | 堆区（Wi-Fi / HTTP / DNS 用） |
| | `CONFIG_MAIN_STACK_SIZE=8192` 等 | 各线程栈（配网需调大） |

---

## 使用说明

1. **首次上电配网**：设备以 AP `ESP32-AP` 广播（IP `192.168.4.1`）。手机/电脑连接该热点，浏览器访问配置页提交家庭 Wi-Fi 的 SSID 与密码。
2. **自动续连**：凭据写入 NVS，之后每次上电自动连接已存网络，无需重复配网。
3. **云服务**：连接 MQTT broker 后自动上报传感器数据，可订阅控制主题下发指令。
4. **本地交互**：通过 OLED 界面查看实时数据与状态；实体按键控制风扇起停；RGB 灯显示当前运行/告警状态。

---

## MQTT 协议

在 `modules/services/mqtt/app_mqtt.h` 中配置（Broker 地址、端口、客户端 ID、账号密码、主题）：

| 项 | 默认值 |
| --- | --- |
| Broker | `192.168.31.191:1883` |
| Client ID | `esp32s3_app_logic` |
| 订阅主题（控制下发） | `device/esp32/command` |
| 发布主题（数据上报） | `sensor/data` |

**下行控制命令**（JSON）：

```json
{
  "power": true,
  "fan_speed": 2,
  "mode": "manual"
}
```

| 字段 | 取值 | 说明 |
| --- | --- | --- |
| `power` | `true` / `false` | 总开关 |
| `fan_speed` | `0~3` | 0=关闭，1=低，2=中，3=高 |
| `mode` | `auto` / `manual` / `night` | 工作模式 |

接收后通过 `control_set_mode` / `control_set_fan_cmd` 等接口下发到控制中心执行。

---

## 工程历史

| 提交 | 说明 |
| --- | --- |
| 抽离 GUI 层 | 将显示逻辑从主程序独立为模块 |
| 合入配网机制 / 静态资源改动态 | 引入 Web 配网，配网页资源改为动态保证释放彻底 |
| 合入 MQTT 相关 | 接入云平台通信 |
| 合入 DC01 驱动 / 数据中转 | 引入传感器数据中心框架 |
| 合入 TOVC-301 | 新增空气质量传感器 |
| 合入 AHT10 | 温湿度采集并优化 Sensor Center 数据结构 |
| 合入风扇驱动 | 加入 PWM 风扇调速 |
| 整合 UI 和数据 | GUI 与数据打通 |
| 基本实现 / 修复 WIFI 图标 | 系统主体完成，修复 Wi-Fi 状态图标显示 |
| 增加指示灯 | 新增 RGB 状态灯 |
| 加入物理风扇起停按钮 | 新增实体按键控制 |

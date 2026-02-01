cmake_minimum_required(VERSION 3.20.0)

list(APPEND ZEPHYR_EXTRA_MODULES ${CMAKE_CURRENT_SOURCE_DIR}/../modules/aht10)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(u8g2_zephyr_app)

# 添加 U8G2 库源码路径
set(U8G2_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/modules/display/u8g2)
set(U8G2_CSRC ${U8G2_ROOT}/csrc)

# 添加Zephyr兼容层 和 GUI 文件
set(ZEPHYR_U8G2 ${CMAKE_CURRENT_SOURCE_DIR}/modules/display/u8g2_zephyr_port)
set(GUI ${CMAKE_CURRENT_SOURCE_DIR}/modules/display/gui)

# 自动配网相关
set(ZEPHYR_WIFI_PROV ${CMAKE_CURRENT_SOURCE_DIR}/modules/services/wifi_prov)

# MQTT相关
set(ZEPHYR_MQTT ${CMAKE_CURRENT_SOURCE_DIR}/modules/services/mqtt)

# 传感器中心
set(SENSOR_CENTER ${CMAKE_CURRENT_SOURCE_DIR}/modules/services/sensor_center)

# DC01 传感器驱动
set(DC01_DRIVER ${CMAKE_CURRENT_SOURCE_DIR}/modules/drivers/dc01)

# TOVC-301 传感器驱动
set(TOVC_301_DRIVER ${CMAKE_CURRENT_SOURCE_DIR}/modules/drivers/tovc_301)

# AHT10 传感器 APP
set(AHT10_DRIVER ${CMAKE_CURRENT_SOURCE_DIR}/modules/drivers/aht10_app)

# 风扇驱动
set(FAN_DRIVER ${CMAKE_CURRENT_SOURCE_DIR}/modules/drivers/fan)

# RGB PWM LED 驱动
set(RGB_PWM_LED_DRIVER ${CMAKE_CURRENT_SOURCE_DIR}/modules/drivers/rgb_led)

# 控制中心
set(CONTROL_CENTER ${CMAKE_CURRENT_SOURCE_DIR}/modules/services/control_center)

# 状态指示服务
set(STATUS_INDICATOR ${CMAKE_CURRENT_SOURCE_DIR}/modules/services/status_indicator)

# 头文件搜索路径
target_include_directories(app PRIVATE 
    ${U8G2_CSRC} 
    ${GUI} 
    ${ZEPHYR_U8G2} 
    ${ZEPHYR_WIFI_PROV} 
    ${ZEPHYR_MQTT} 
    ${SENSOR_CENTER}
    ${DC01_DRIVER}
    ${TOVC_301_DRIVER}
    ${AHT10_DRIVER}
    ${FAN_DRIVER}
    ${CONTROL_CENTER}
    ${STATUS_INDICATOR}
    ${RGB_PWM_LED_DRIVER}
)

# 移除这里的 U8G2_16BIT 宏定义，因为源码 u8g2.h 已经定义了，避免报 Redefined 警告
target_compile_definitions(app PRIVATE
    U8X8_USE_PINS
)

# 添加 U8G2 库源码文件(此处没有全部包含，若报错请根据报错信息补充对应文件)
target_sources(app PRIVATE
    src/main.c

    # --- 基础核心 ---
    ${U8G2_CSRC}/u8g2_setup.c
    ${U8G2_CSRC}/u8g2_d_setup.c
    ${U8G2_CSRC}/u8g2_d_memory.c
    ${U8G2_CSRC}/u8g2_buffer.c
    
    # --- U8x8 核心 ---
    ${U8G2_CSRC}/u8x8_display.c
    ${U8G2_CSRC}/u8x8_setup.c
    ${U8G2_CSRC}/u8x8_8x8.c
    ${U8G2_CSRC}/u8x8_byte.c
    ${U8G2_CSRC}/u8x8_gpio.c
    ${U8G2_CSRC}/u8x8_cad.c
    ${U8G2_CSRC}/u8x8_string.c
    ${U8G2_CSRC}/u8x8_fonts.c
    ${U8G2_CSRC}/u8x8_selection_list.c # u8x8 基础支持

    # --- 绘图逻辑 ---
    ${U8G2_CSRC}/u8g2_font.c
    ${U8G2_CSRC}/u8g2_font.c
    ${U8G2_CSRC}/u8g2_bitmap.c
    ${U8G2_CSRC}/u8g2_hvline.c
    ${U8G2_CSRC}/u8g2_ll_hvline.c      # u8g2_ll_hvline_vertical_top_lsb
    ${U8G2_CSRC}/u8g2_intersection.c   # u8g2_IsIntersection
    ${U8G2_CSRC}/u8g2_box.c            # u8g2_DrawBox 和 u8g2_DrawFrame

    # --- 字体与字符处理 ---
    ${U8G2_CSRC}/u8x8_u8toa.c          # 基础转换
    ${U8G2_CSRC}/u8x8_u16toa.c
    ${U8G2_CSRC}/u8g2_fonts.c          # u8g2_font_ncenB08_tr

    # --- 显示驱动 ---
    ${U8G2_CSRC}/u8x8_d_ssd1306_128x64_noname.c

    # --- GUI 相关 ---
    ${ZEPHYR_U8G2}/u8g2_zephyr_port.c
    ${GUI}/gui_app.c
    ${GUI}/gui.c

    # --- WiFi 配网相关 ---
    ${ZEPHYR_WIFI_PROV}/wifi_prov_app.c
    ${ZEPHYR_WIFI_PROV}/wifi_provisioning.c

    # --- MQTT 相关 ---
    ${ZEPHYR_MQTT}/app_mqtt.c
    ${ZEPHYR_MQTT}/mqtt_backend.c

    # --- 传感器中心 ---
    ${SENSOR_CENTER}/sensor_center.c

    # --- DC01 传感器驱动 ---
    ${DC01_DRIVER}/dc01_app.c

    # --- TOVC-301 传感器驱动 ---
    ${TOVC_301_DRIVER}/tovc_301.c

    # --- AHT10 传感器 APP ---
    ${AHT10_DRIVER}/aht10_app.c

    # --- 风扇驱动 ---
    ${FAN_DRIVER}/fan.c

    # --- 控制中心 ---
    ${CONTROL_CENTER}/control_center.c

    # --- 状态指示服务 ---
    ${STATUS_INDICATOR}/status_indicator.c

    # --- RGB PWM LED 驱动 ---
    ${RGB_PWM_LED_DRIVER}/rgb_led.c
)
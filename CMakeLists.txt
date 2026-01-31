cmake_minimum_required(VERSION 3.20.0)

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

# 头文件搜索路径
target_include_directories(app PRIVATE ${U8G2_CSRC} ${GUI} ${ZEPHYR_U8G2} ${ZEPHYR_WIFI_PROV})

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
)
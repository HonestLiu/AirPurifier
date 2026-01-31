# Custom Zephyr funtion that imports the button/ subdirectory if the Kconfig
# option AHT10 is defined
add_subdirectory_ifdef(CONFIG_DC01 dc01)
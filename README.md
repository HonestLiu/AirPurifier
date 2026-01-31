**简述:** 演示U8g2库的使用

**设备:** STM32F407VGT6 & ESP32S3

**时间:** 

- Create: 2026.1.26

- Update1: 2026.1.28 新增ESP32S3支持

- Update2: 2026.1.30 新增lopaka.app设计UI的移植


**编译:**

```shell
# STM32F407VGT6
west build -p always -b stm32f4_disco . -- -DDTC_OVERLAY_FILE=boards/stm32f4_disco.overlay
# ESP32S3
west build -p always -b esp32s3_devkitc/esp32s3/procpu -- -DDTC_OVERLAY_FILE=boards/esp32s3_devkitc.overlay
python -m esptool --port "/dev/ttyACM0" --chip auto --baud 921600 --before default-reset --after hard_reset write_flash -u --flash-size detect 0x0 ./build/zephyr/zephyr.bin
```


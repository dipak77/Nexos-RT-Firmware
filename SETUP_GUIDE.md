# Smart Device Platform - Complete Setup Guide
## ESP32-S3 + GC9A01 240x240 Round + LVGL 9.5 + Microkernel + Premium UI
### Version 1.0.0 Premium Production

## Quick Start (5 minutes)

### 1. Prerequisites (Windows)
- Git
- VS Code + ESP-IDF Extension v1.8+
- ESP-IDF Installation Manager (GUI) -> Install ESP-IDF v6.1 (release-v6.1)
- Python 3.11 (managed by IDF)
- USB drivers: CP210x or CH340 (check Device Manager)

Verify:
```
idf.py --version
# Expected: ESP-IDF v6.1
python --version
git --version
```

### 2. Get Code
```
git clone <your-repo> smart_device_firmware
cd smart_device_firmware
```

### 3. Set Target & Dependencies
```
idf.py set-target esp32s3
idf.py add-dependency "espressif/esp_lcd_gc9a01^2.0.4"
idf.py add-dependency "espressif/esp_lvgl_port^2.9.0"  # Only for reference, we use our own lvgl_adapter
idf.py add-dependency "lvgl/lvgl^9.5.0"
```

Locked versions:
- esp_lcd_gc9a01 2.0.4
- esp_lvgl_port 2.9.0 (not used in final, we use lvgl_adapter direct)
- LVGL 9.5.0
- ESP-IDF 6.1.x (July 31 2026 release)

### 4. Hardware Wiring

The round GC9A01 is **4-wire SPI**, not I2C. Silk SCL/SDA are SPI clock/MOSI. This 7-pin board has no BLK pin — use **5V on VCC**.

| GC9A01 | ESP32-S3 | Notes |
|---|---|---|
| VCC | **5V** | Backlight is tied to VCC; 3V3 often looks off |
| GND | GND | |
| SCL | GPIO12 | SPI CLK (not I2C) |
| SDA | GPIO11 | SPI MOSI (not I2C) |
| DC | GPIO10 | Data/command |
| CS | GPIO9 | Chip select |
| RST | GPIO14 | Reset |

I2C expansion (sensors/touch) is GPIO 16/17. Do not wire the LCD SDA/SCL to GPIO 5/6.

Do NOT use GPIO19/20 (USB D-/D+), GPIO43/44 (UART0).

### 5. Build & Flash
```
idf.py build
idf.py -p COM7 flash monitor
# Replace COM7 with your port from Device Manager
```

Expected boot log:
```
================================================
 Smart Device Firmware - Next OS
 Firmware : 1.0.0
 Kernel   : Next OS 0.8.0
 Hardware : S3-DK-V1.1
...
[PASS] NVS initialized
[PASS] SPI bus initialized MOSI=11 CLK=12
[PASS] GC9A01 write path initialized 240x240
[PASS] LVGL 9.5 direct adapter initialized (mk_mutex, mk_timer 1ms tick)
[PASS] Next OS v0.8.0 initialized
...
SYSTEM READY - Next OS RUNNING
device> 
```

### 6. Test Commands
```
help
status
version
wifi scan
wifi connect MySSID MyPass
time status
self-test
kernel status
display brightness 75
reboot
```

### 7. Premium UI
Round 240x240 glassmorphism:
- Top pills WiFi/BLE with glow
- Time 12:24 + seconds + AM/PM cyan
- Date 28 AUG 2026 cyan tracking
- System OK chip green with glow
- Command glass card with PASS/FAIL
- Bottom FW + uptime + heap bar
- Boot animation: fade + arc sweep 1200ms

See components/gui/gui_dashboard.cpp and gui_theme.h

## Architecture (Next OS)

```
Product App (mk_* only)
  -> SDK (mk_* only)
    -> Next OS
      -> ESP32-S3 chip support package (ESP-IDF / Arduino-ESP32 drivers)
        -> ESP32-S3
```

Application includes `mk.h` only.

Core affinity:
- Core 0: chip support (WiFi/BLE/TCP/IP)
- Core 1: Next OS app tasks (GUI P7, COMMAND P6, DIAGNOSTICS P3)

## Project Structure
```
main/app_main.cpp (uses mk_task_create, never xTaskCreate)
components/
  microkernel/ (Next OS: mk_kernel, mk_task, mk_queue, mk_mutex with PI, mk_timer, mem pools)
  lvgl_adapter/ (own LVGL direct, no esp_lvgl_port)
  platform/ (ESP32-S3 shim)
  board/ (pins, config)
  hal/ (spi, i2c, gpio, usb)
  display/ (GC9A01 driver)
  gui/ (premium dashboard glassmorphism)
  command/ (unified registry)
  connectivity/ (wifi, ble)
  time_service/ (SNTP IST-5:30)
  storage/ (NVS)
  diagnostics/ (self-test)
  ota/ (HTTPS OTA)
```

## OTA
```
device> ota_status
# OTA flow: Download -> ota_1 -> Verify -> Restart -> Self-test -> PASS Accept / FAIL Rollback
# Display shows: FIRMWARE UPDATE 67% + DO NOT POWER OFF
```

## Production Checklist V1.0.0
- [x] Board bring-up 100/100 boots
- [x] GC9A01 240x240 + LVGL 9.5 direct + premium UI
- [x] Microkernel v0.8.0 with PI mutex
- [x] Command engine PASS/FAIL on LCD
- [x] WiFi + BLE + USB CDC unified
- [x] NVS + Time SNTP IST
- [x] OTA + rollback
- [ ] 72h soak + watchdog
- [ ] Secure Boot + Flash Encryption (enable at V0.9)

## Troubleshooting
- No display: LCD is SPI not I2C. CLK 12 MOSI 11 CS 9 DC 10 RST 14, VCC=5V, 2MHz prototype clock. Do not use GPIO 5/6 for the LCD.
- WiFi not connecting: Check ssid/pass in NVS, `factory_reset`
- Build error: `idf.py fullclean`, set-target esp32s3 again
- LVGL blank: Check buf alloc DMA, `heap_caps_malloc`

## Support
Docs: docs/architecture.md, docs/architecture_microkernel.md, docs/hardware.md, docs/commands.md

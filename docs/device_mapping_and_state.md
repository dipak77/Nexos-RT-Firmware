# Device Mapping & State Reference — Nexos-RT v1.2

> **Operating System:** **Nexos-RT v1.2** (Production Hardened Standalone Microkernel Runtime).
> **Canonical Reference:** See `docs/COMPLETE_SYSTEM_REFERENCE.md`.
> **Wiring:** 7-wire SPI. VCC is **3V3**; CS is GPIO9 and RST is GPIO14.

## 1. Hardware Identity

- **MCU board:** ESP32-S3-DevKitC-1 **v1.1** (Espressif)
- **Module:** ESP32-S3-WROOM-1 **N8R8** (silk `MCN8R8`) — 8 MB flash, 8 MB octal PSRAM, 342 KB SRAM
- **USB bridge:** Silicon Labs CP210x (VID:PID `10C4:EA60`) on UART0
- **Display:** 1.28" round TFT, **240x240**, driver **GC9A01**
- **Display power:** TFT VER1.0 is a **3.3 V** module. Its backlight is tied to VCC.

## 2. Pin Mapping (7-Wire Setup)

Display header order: `VCC GND SCL SDA DC CS RST`.

| Display Pin | Function | Header J1 Pin | ESP32-S3 GPIO | Status / Wiring Rule |
|---|---|---|---|---|
| **VCC** | Power + Backlight | J1-1 or J1-2 (`3V3`) | **3.3 V** | Never connect to USB 5 V |
| **GND** | Ground | J1-22 (`G`) | **GND** | Ground |
| **SCL / CLK** | SPI Clock | J1-18 (`12`) | **GPIO 12** | Direct connection |
| **SDA / DIN** | SPI MOSI | J1-17 (`11`) | **GPIO 11** | Direct connection |
| **DC** | Data/Command | J1-16 (`10`) | **GPIO 10** | Direct connection |
| **CS** | Chip Select | J1-15 (`9`) | **GPIO 9** | Direct connection; active low |
| **RST** | Reset | J1-20 (`14`) | **GPIO 14** | Direct connection; active low |

### Header J1 Silk Order (Antenna/Top -> USB/Bottom)
`3V3, 3V3, RST, 4, 5, 6, 7, 15, 16, 17, 18, 8, 3, 46, 9, 10, 11, 12, 13, 14, 5V, G`


GPIO 9/10/11/12/13/14 are the last six numbered holes above 5V. The previous listing
(`3V3, RST, 4, 5, 6, 7, 15, 13, 12, ...`) was wrong and will wire SCL/SDA onto GPIO 16/17.

## 3. Firmware state

- **Hardware-test build env:** `platformio.ini` -> `[env:esp32s3_arduino]` (Arduino framework,
  PlatformIO). This small image provides the serial console and deterministic LCD color test.
- **Active source:** `src/main.cpp` — a dashboard (Arduino/Adafruit_GFX) that renders
  SPI/OS state, uptime, health, heap, plus a serial `device>` console. It runs a
  red/green/blue startup test before the branded splash and dashboard.
- **Canonical product:** `main/app_main.cpp` is the ESP-IDF LVGL 9.5 firmware. Both the
  Arduino hardware-test image and the full ESP-IDF image build successfully on this PC.
- **Code fixes already applied this session (in repo):**
  - `main/app_main.cpp`: NVS/Time/Diagnostics/OTA init made non-fatal (a single failure no
    longer aborts boot before the UI draws).
  - `components/display/display_gc9a01.cpp`: BGR -> RGB color order.
  - `components/lvgl_adapter/lvgl_adapter.cpp`: removed dead `RGB565_SWAPPED` branch.
  - `components/hal/usb_hal.cpp`: removed non-existent `esp_vfs_cdcacm_register()`.
  - `components/microkernel/core/mk_task.c`: Next OS priority mapping.
- **Display bring-up configuration (Arduino env):** hardware SPI at a conservative 2 MHz,
  explicit CS on GPIO9 and hard reset on GPIO14, followed by red/green/blue/black frames
  before the dashboard. The display supply must be 3.3 V.
- **Serial console commands:** `help, version, status, wifi connect <ssid> <pass>, wifi status,
  ble start/stop, time status/sync, display test, display brightness <0-100>, self-test,
  reboot, factory_reset`.

## 4. Last boot log (COM5, 115200, captured 2026-08-28)

```
ESP-ROM:esp32s3-20210327
Build:Mar 27 2021
rst:0x1 (POWERON),boot:0x8 (SPI_FAST_FLASH_BOOT)
SPIWP:0xee
mode:DIO, clock div:1
load:0x3fce3808,len:0x4bc
load:0x403c9700,len:0xbd8
load:0x403cc700,len:0x2a0c
entry 0x403c98d0
================================================
 Smart Device Firmware - DASHBOARD (Arduino)
 GC9A01 240x240 4-wire SPI (not I2C)
 Pins: SCL=12 SDA=11 DC=10 CS=9 RST=14 VCC=3V3
================================================
device>
```
(The firmware reaches `device>` with no crash — SPI/panel init did not abort the app.)

## 5. OPEN ISSUE — display shows blue scanlines (backlight ON, no pixels)

**Symptom:** backlight lit, screen shows bright blue noise/scanlines, never the dashboard.
That pattern is GC9A01 GRAM after SLPOUT+INVON+DISPON with **no successful RAMWR**. Commands
reached the panel; pixel DMA did not.

**Fixes applied 2026-08-29:**
- IDF driver no longer holds CS permanently low (`GPIO_NUM_NC`). CS is GPIO9 and toggles.
- IDF driver fills GRAM **before** DISPON so a failed pixel path stays dark, not snow.
- Arduino TFT_eSPI now sets `USE_HSPI_PORT` (required on ESP32-S3 / Arduino 3.x).
- `main/CMakeLists.txt` required a non-existent `hal` component; it now requires `device_hal`.

**Hardware check against the photos:**
- Left header (antenna -> USB): `3V3 3V3 RST 4 5 6 7 15 16 17 18 8 3 46 9 10 11 12 13 14 5V G`
- SCL -> silk **12**, SDA -> silk **11**, DC -> **10**, CS -> **9**, RST -> **14**, VCC -> **3V3**, GND -> **G**
- CS and RST must both be jumpered; do not rely on optional module resistors.

## 6. Quick recovery commands
```
# rebuild + flash (Arduino env, current PC)
pio run -e esp32s3_arduino
pio run -e esp32s3_arduino -t upload --upload-port COM5
# serial monitor
pio device monitor -p COM5 -b 115200
# erase + reflash from scratch (if partition/NVS suspected)
pio run -e esp32s3_arduino -t upload --upload-port COM5   # after `esptool.py erase_flash`
```

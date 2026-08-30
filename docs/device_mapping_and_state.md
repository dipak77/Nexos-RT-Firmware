# Device Mapping & State Reference

> **Superseded as the live spec.** Use `docs/COMPLETE_SYSTEM_REFERENCE.md`.
> Product OS is **Next OS** only. CS and RST stay unplugged; VCC is USB-end `5V`.
>
> The notes below are a bring-up log (including the old CS=9 / RST=14 experiment).
> Do not treat them as current wiring or OS policy.

> Generated during bring-up of the GC9A01 240x240 round display on ESP32-S3.

## 1. Hardware identity (from esptool + photos)

- **MCU board:** ESP32-S3-DevKitC-1 **v1.1** (Espressif)
- **Module:** ESP32-S3-WROOM-1 **N8R8** (silk `MCN8R8`) — 8 MB flash, 8 MB octal PSRAM
- **USB bridge:** Silicon Labs CP210x (VID:PID `10C4:EA60`), **COM5** on this PC
- **Display:** 1.28" round TFT, **240x240**, driver **GC9A01** (silk on back: `1.28" TFT VER1.0 IC: GC9A01 240*240`)
- **Display quirk (from module silk):** *"CS and RST may be left open (R8 pulls CS low)"*.
  The panel has **NO BLK pin** — backlight LED is tied to VCC. So VCC must be **5V** or
  the backlight stays dark even though the MCU runs.
- **esptool read:** `ESP32-S3 (QFN56) rev v0.2, Wi-Fi BT5 LE, Dual Core + LP Core, 240MHz,
  Embedded PSRAM 8MB, 40MHz crystal, USB-Serial/JTAG, MAC 30:ed:a0:17:6b:7c`

## 2. Pin mapping (DISPLAY -> ESP32-S3) — CONFIRMED by user 2026-08-28

Display header order on the blue board (read by user): `VCC GND SCL SDA DC CS RST`.

| Display pin | Function        | DevKit hole (silkscreen) | GPIO | Firmware constant | Notes |
|-------------|-----------------|--------------------------|------|-------------------|-------|
| VCC         | Power + backlight | `5V` hole (end of left header, next to GND) | — | — | **MUST be 5V.** 3V3 -> backlight dark. |
| GND         | Ground          | any `GND` hole           | — | — | — |
| SCL / CLK   | SPI clock       | hole `GPIO12`            | 12 | `PIN_SCLK = 12` | FSPI CLK |
| SDA / DIN   | SPI MOSI        | hole `GPIO11`            | 11 | `PIN_MOSI = 11` | FSPI MOSI |
| DC          | Data/Command    | hole `GPIO10`            | 10 | `PIN_DC   = 10` | any GPIO |
| CS          | Chip select     | hole `GPIO9`             | 9  | `PIN_CS   = 9`  | any GPIO (module pulls down via R8) |
| RST         | Reset           | hole `GPIO14` (next to 5V) | 14 | `PIN_RST = 14` | Active-low. Must be wired. A floating/wrong RST pin held the panel in reset during earlier bring-up. |
| BL          | Backlight       | **LEFT OPEN**            | — | — | Tied to VCC internally. |

All signal pins (9/10/11/12/14) are on the ESP32-S3 **left header** (USB at the bottom).
GPIO11/12 are native FSPI MOSI/CLK. GPIO9 is CS (drive it; do not rely on R8 alone).

### DevKitC-1 v1.1 left-header silkscreen (antenna/top -> USB/bottom)
`3V3, 3V3, RST, 4, 5, 6, 7, 15, 16, 17, 18, 8, 3, 46, 9, 10, 11, 12, 13, 14, 5V, G`

GPIO 9/10/11/12/13/14 are the last six numbered holes above 5V. The previous listing
(`3V3, RST, 4, 5, 6, 7, 15, 13, 12, ...`) was wrong and will wire SCL/SDA onto GPIO 16/17.

## 3. Firmware state

- **Active build env:** `platformio.ini` -> `[env:esp32s3_arduino]` (Arduino framework,
  PlatformIO). This is the ONLY toolchain that builds on the current PC.
- **Active source:** `src/main.cpp` — a REAL dashboard (Arduino/Adafruit_GFX) that renders
  WiFi/BLE pills, time cluster, status chip, command card, heap bar, plus a serial `device>`
  console. It flashes WHITE on boot then draws a light-background dashboard.
- **Canonical product (not built here):** `main/app_main.cpp` is the ESP-IDF LVGL 9.5 firmware.
  It does NOT compile on this PC (PlatformIO 7.0.1 bundles ESP-IDF 6.0.1 + CMake 3.30.2;
  CMake 3.30 made `define_property`/`add_library` non-scriptable in `cmake -P` mode, which
  ESP-IDF's `kconfig.cmake`/`build.cmake` use. Only `tool-cmake 3.30.2` exists in the
  registry — no downgrade. Build blocked here; needs a machine with ESP-IDF 6.1 installed.)
- **Code fixes already applied this session (in repo):**
  - `main/app_main.cpp`: NVS/Time/Diagnostics/OTA init made non-fatal (a single failure no
    longer aborts boot before the UI draws).
  - `components/display/display_gc9a01.cpp`: BGR -> RGB color order.
  - `components/lvgl_adapter/lvgl_adapter.cpp`: removed dead `RGB565_SWAPPED` branch.
  - `components/hal/usb_hal.cpp`: removed non-existent `esp_vfs_cdcacm_register()`.
  - `components/microkernel/core/mk_task.c`: Next OS priority mapping.
- **Display bring-up settings tried (Arduino env):** SPI clock 8 MHz then 4 MHz; explicit
  hard-reset pulse on RST (then RST disabled/open); white flash diagnostic; light theme.
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
 Pins: SCL=12 SDA=11 DC=10 CS=9  VCC=5V  RST=open
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
- SCL -> silk **12**, SDA -> silk **11**, DC -> **10**, CS -> **9**, RST -> **14**, VCC -> **5V**, GND -> **G**
- CS and RST must both be jumpered. R8 pull-down is not enough for RAMWR on this module.

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

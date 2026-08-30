# Smart Device Firmware — Complete System Reference

**Status:** live on hardware as of 2026-08-30
**Firmware version:** 1.2.0
**Product OS:** **Nexos-RT** v1.2.0 (`mk_*` API) — the only application operating system
**Active build:** PlatformIO `[env:esp32s3_arduino]`
**UART:** Silicon Labs CP210x, **COM5**, 115200 baud
**MAC (this board):** `1c:db:d4:9c:1d:78`

This file is the canonical description of the project: hardware, corrected wiring, **Nexos-RT**, boot flow, pin map, commands, and source layout. The TFT VER1.0 module is a 3.3 V part; CS and RST are explicitly wired to GPIO9 and GPIO14.

**Product policy:** Nexos-RT is the only OS name. Dual-kernel switch language is withdrawn.

---

## Table of contents

1. [Project overview](#1-project-overview)
2. [Hardware identity](#2-hardware-identity)
3. [Official DevKitC-1 v1.1 header map](#3-official-devkitc-1-v11-header-map)
4. [Working display wiring (proven)](#4-working-display-wiring-proven)
5. [Pin mapping (firmware vs silk)](#5-pin-mapping-firmware-vs-silk)
6. [Why this wiring is required](#6-why-this-wiring-is-required)
7. [System architecture](#7-system-architecture)
8. [Next OS](#8-next-os)
9. [Two firmware tracks](#9-two-firmware-tracks)
10. [Boot sequence](#10-boot-sequence)
11. [Tasks, priorities, cores](#11-tasks-priorities-cores)
12. [Display pipeline](#12-display-pipeline)
13. [Source layout](#13-source-layout)
14. [Serial console](#14-serial-console)
15. [Build, flash, monitor](#15-build-flash-monitor)
16. [Official documents](#16-official-documents)
17. [Known pitfalls](#17-known-pitfalls)

---

## 1. Project overview

Circular smart-display firmware for:

| Item | Value |
|---|---|
| MCU board | ESP32-S3-DevKitC-1 **v1.1** |
| Module | ESP32-S3-WROOM-1 **N8R8** (silk `MCN8R8`) |
| CPU | Dual Xtensa LX7 @ 240 MHz |
| Flash | 8 MB Quad SPI |
| PSRAM | 8 MB Octal SPI (GPIO35/36/37 reserved inside the module) |
| Display | 1.28" round TFT VER1.0, **GC9A01**, 240×240, 4-wire SPI |
| RGB LED | Addressable LED on **GPIO38** (v1.1; v1.0 used GPIO48) |
| Product OS | **Nexos-RT** v1.2.0 — only application OS |
| Product GUI (IDF track) | LVGL 9.5 + glassmorphism dashboard |
| Bring-up GUI (Arduino track, **what is flashed**) | Adafruit GC9A01A hardware SPI dashboard at 2 MHz |

Design goal: every application thread, sleep, mutex, queue, and timer goes through **Next OS** (`mk_*`). Product code does not name or select a second kernel. Wi-Fi / BLE / USB / OTA stay on the Espressif chip support package (ESP-IDF / Arduino-ESP32), which is a vendor driver runtime — not a product OS.

---

## 2. Hardware identity

### 2.1 DevKit

- Board: ESP32-S3-DevKitC-1 v1.1
- Ordering code: `ESP32-S3-DevKitC-1-N8R8`
- Module: ESP32-S3-WROOM-1-N8R8, PCB antenna
- USB-to-UART: Micro-USB labeled **UART**, CP210x, up to 3 Mbps
- Native USB: Micro-USB labeled **USB** (ESP32-S3 USB-OTG, GPIO19/20)
- 5 V → 3.3 V LDO on board
- 3.3 V power-on LED (small red): on whenever USB power is present
- RGB LED: GPIO38
- Buttons: **BOOT** (download) and **RESET**
- Chip: ESP32-S3 QFN56 rev 0.2, 40 MHz crystal

GPIO voltage is **3.3 V**. There is **no 5 V GPIO** on the chip. The only 5 V on the board is USB VBUS broken out as silk **`5V`**.

### 2.2 Display module

Back silk:

- `1.28" TFT VER1.0`
- `240*240`
- `IC: GC9A01`
- Header marked **SPI**
- Pin order from VCC toward RST: **`VCC GND SCL SDA DC CS RST`**
- Some revisions fit an optional CS resistor, but the reliable wiring drives CS and RST explicitly.
- **No BLK pin.** Backlight LED is tied to VCC. Feed the module **3.3 V**, never USB 5 V.

`SCL` / `SDA` on this PCB are **SPI CLK and SPI MOSI**, not I2C. Proof: extra **DC** and **CS** pins exist only on SPI panels.

### 2.3 Pins that must never be used for the LCD

| GPIO | Why |
|---|---|
| 35, 36, 37 | Octal PSRAM inside WROOM-1-N8R8 |
| 19, 20 | Native USB D− / D+ |
| 43, 44 | UART0 TX/RX (console / flash) |
| 0, 3, 45, 46 | Strapping |
| 38 | RGB LED |
| 21 | GPIO21 on **J3** (right row). **Not 5 V.** |

---

## 3. Official DevKitC-1 v1.1 header map

Source: `hardware screenshot/esp-dev-kits-en-master-esp32s3.pdf` Chapter 1, Header Block, and Fig. 4 pin layout. Same tables:  
https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-devkitc-1/user_guide_v1.1.html

Hold the board **antenna / metal can at the top, USB ports at the bottom.**

**J1 = left row.** Pin 1 is the antenna end. Pin 22 is the USB end.

| J1 No. | Silk | Function |
|---|---|---|
| 1 | 3V3 | 3.3 V (antenna end) |
| 2 | 3V3 | 3.3 V |
| 3 | RST | Chip EN (ESP reset, **not** LCD RST) |
| 4 | 4 | GPIO4 |
| 5 | 5 | GPIO5 |
| 6 | 6 | GPIO6 |
| 7 | 7 | GPIO7 |
| 8 | 15 | GPIO15 |
| 9 | 16 | GPIO16 |
| 10 | 17 | GPIO17 |
| 11 | 18 | GPIO18 |
| 12 | 8 | GPIO8 |
| 13 | 3 | GPIO3 (strap) |
| 14 | 46 | GPIO46 (strap) |
| 15 | 9 | GPIO9, FSPIHD |
| 16 | 10 | GPIO10, FSPICS0 |
| 17 | 11 | GPIO11, **FSPID (MOSI)** |
| 18 | 12 | GPIO12, **FSPICLK** |
| 19 | 13 | GPIO13, FSPIQ (MISO, unused) |
| 20 | 14 | GPIO14, FSPIWP |
| **21** | **5V** | **5 V from USB VBUS** |
| **22** | **G** | Ground |

**J3 = right row** (antenna-end G down to USB-end G): TX/RX, GPIO1–2, 42–35, 0, 45, 48, 47, **GPIO21**, USB 20/19, G, G.

Chip datasheet native SPI2 / FSPI IOMUX (matches J1 silk 10–14):

| GPIO | FSPI name | LCD use |
|---|---|---|
| 11 | FSPID | MOSI / display SDA |
| 12 | FSPICLK | CLK / display SCL |
| 10 | FSPICS0 | DC (command/data) |
| 13 | FSPIQ | unused (write-only panel) |
| 9 | FSPIHD | LCD CS |
| 14 | FSPIWP | LCD RST |

Power options (mutually exclusive): USB-to-UART and/or ESP USB; **5V + G**; or **3V3 + G**.

---

## 4. Correct display wiring

Use the signal pins near the USB end, plus either DevKit `3V3` pin for display VCC. The USB `5V` pin is not a valid supply for this TFT module.

### 4.1 Desk orientation (same as the working photo)

- Metal can / antenna = **left**
- Two USB sockets = **right**
- Display sits **below** the board
- Use the **bottom** row of pins (J1). It has `3V3` at the antenna and the LCD signal GPIOs near the UART USB.

Do **not** use the top row (`TX` `RX` `21`). Silk `21` there is GPIO21.

### 4.2 Count from the UART USB, bottom row

Start at the hole **closest to the UART socket** and walk toward the antenna:

| Count from UART | Silk | Display wire |
|---|---|---|
| 1 | **G** | **GND** |
| 2 | **5V** | **empty; never TFT VCC** |
| 3 | **14** | **RST** |
| 4 | **13** | **empty** |
| 5 | **12** | **SCL** |
| 6 | **11** | **SDA** |
| 7 | **10** | **DC** |
| 8 | **9** | **CS** |
| 21 or 22 | **3V3** | **VCC** |

```
antenna (left)                                    UART USB (right)
3V3 …  9   10   11   12   13   14   5V   G
 │      │    │    │    │          │          │
 VCC    CS   DC   SDA  SCL        RST        GND
```

### 4.3 Round PCB — seven wires

| Display silk | ESP silk | Notes |
|---|---|---|
| **VCC** | **3V3** | 3.3 V module supply; never USB 5 V. |
| **GND** | **G** | Common ground. |
| **SCL** | **12** | SPI clock |
| **SDA** | **11** | SPI MOSI. Not I2C. |
| **DC** | **10** | Command / data |
| **CS** | **9** | Active-low chip select, driven for every transaction. |
| **RST** | **14** | Active-low reset, pulsed during initialization. |

### 4.4 Breadboard rule

The DevKit sits in the breadboard. Each ESP pin is common only with the **other holes in that same 5-hole row**.

- Plug each Dupont in the **same row** as the counted pin.
- Do not use the long red/blue power rails unless you have verified they are tied to `3V3`/`G`.
- One row off = firmware still runs (RGB LED / serial alive), glass stays flat blue.

Unplug USB before moving VCC. Do not hot-swap display power.

---

## 5. Pin mapping (firmware vs silk)

### 5.1 Arduino track (flashed, working)

File: `src/main.cpp`

| Signal | Define | Value |
|---|---|---|
| MOSI | `TFT_MOSI` | 11 |
| SCLK | `TFT_SCLK` | 12 |
| DC | `TFT_DC` | 10 |
| CS | `TFT_CS` | **9** |
| RST | `TFT_RST` | **14** |

Driver: `Adafruit_GC9A01A(&SPI, DC, CS, RST)` — hardware FSPI at **2 MHz**. The firmware explicitly binds SCLK 12 and MOSI 11 before calling the driver.

### 5.2 IDF product track (not the image currently on the chip)

File: `components/board/include/board_pins.h`

| Signal | Define | Value |
|---|---|---|
| MOSI | `LCD_PIN_MOSI` | 11 |
| SCLK | `LCD_PIN_SCLK` | 12 |
| DC | `LCD_PIN_DC` | 10 |
| CS | `LCD_PIN_CS` | 9 (driver still toggles CS if this stays 9) |
| RST | `LCD_PIN_RST` | 14 |
| BL | `LCD_PIN_BL` | -1 |
| Pixel clock | `LCD_PIXEL_CLOCK_HZ` | 2 MHz |
| I2C expansion SDA/SCL | `I2C_EXP_SDA` / `I2C_EXP_SCL` | 16 / 17 (**not** the LCD) |

Hardware uses the same CS=9 and RST=14 wiring for both build tracks.

---

## 6. Why this wiring is required


| Symptom | Cause | Fix |
|---|---|---|
| Backlight on, no pixels / snow | 5 V over-voltage, missing CS/RST, or wrong SPI row | Power-cycle at 3.3 V and verify all seven connections |
| Completely black | RST held low or controller damaged | Verify GPIO14 reaches RST and pulses high-low-high |
| No GRAM writes | CS floating or wrong GPIO | Connect CS directly to GPIO9 |
| GPIO21 used as power | J3 silk `21` is GPIO21 | Use a J1 `3V3` pin for VCC |

GPIO9–14 are **not** strapping pins. The initialization sequence deliberately resets the LCD after GPIO setup, so a normal power-up transition on GPIO14 is harmless.

---

## 7. System architecture

```
+------------------------------------------------------------------+
|  Product / bring-up application                                  |
|  Arduino: Adafruit dashboard                                     |
|  IDF:     LVGL 9.5 GUI + CommandService + WiFi/BLE/OTA           |
+------------------------------------------------------------------+
                |  app includes mk.h only
                v
+------------------------------------------------------------------+
|  Nexos-RT v1.2.0                                                |
|  mk_init / mk_task_create_ext / mk_sleep_ms / mk_start           |
|  mutex, semaphore, queue, event, timer, memory pool              |
+------------------------------------------------------------------+
                |
                v
+------------------------------------------------------------------+
|  ESP32-S3 chip support package (ESP-IDF / Arduino-ESP32)         |
|  Vendor drivers: Wi-Fi, BLE, USB, flash, PSRAM                   |
+------------------------------------------------------------------+
                v
+------------------------------------------------------------------+
|  ESP32-S3-WROOM-1-N8R8 + GC9A01 SPI panel                        |
+------------------------------------------------------------------+
```

Layering for the IDF product (`main/app_main.cpp`):

```
APPLICATION
  SystemController
    GUI (LVGL 9.5 via lvgl_adapter)
    CommandService
    Services (WiFi, BLE, Time, OTA, Storage, Diagnostics)
      EventBus
        HAL (SPI, I2C, GPIO, USB)
          BoardConfig
            Nexos-RT (mk_*) + ESP-IDF chip support
```

Core split:

- **Core 0:** chip support (Wi-Fi, BLE, lwIP)
- **Core 1:** GUI, SysMon and CLI (Nexos-RT affinity 1)

There is no second product kernel. Application tasks are created only with `mk_task_create_ext`.

---

## 8. Nexos-RT

**Name:** Nexos-RT
**API prefix:** `mk_*`
**Version:** 1.2.0 (`MK_CONFIG_VERSION_STRING`)
**Source:** `components/microkernel/`
**Dashboard chip:** **Nexos-RT**

### 8.1 What it is

Nexos-RT is the product operating system: a C API (`mk_*`) plus C++ RAII (`mk_cpp.hpp`). Application code includes `mk.h` only. Tasks, sleep, IPC, timers, and pools all go through this API.

Product policy: **Nexos-RT is the only OS.** There is no user-facing kernel switch.

### 8.2 Tree

```
components/microkernel/
  include/          mk.h, mk_config.h, mk_types.h, mk_kernel.h, mk_task.h,
                    mk_mutex.h, mk_semaphore.h, mk_queue.h, mk_event.h,
                    mk_timer.h, mk_memory.h, mk_diagnostics.h, mk_cpp.hpp
  core/             mk_kernel.c, mk_task.c, mk_scheduler.c, mk_idle.c
  ipc/              mk_mutex.c, mk_semaphore.c, mk_queue.c, mk_event.c
  time/             mk_tick.c, mk_timer.c
  memory/           mk_pool.c
  arch/esp32s3/     mk_arch.c, mk_port_shim.c
```

Arduino builds this tree via `tools/pio_kernel_sources.py` (`BuildLibrary` + `env.Prepend(LIBS=…)`).  
IDF builds it as component `microkernel`.

### 8.3 Configuration (`mk_config.h`)

| Macro | Value |
|---|---|
| Version | 1.2.0 |
| Tick | 1000 Hz |
| Max tasks | 16 |
| Max priorities | 8 (1 = idle … 7 = highest) |
| Preemption | on |
| Round-robin | on |
| Mutex PI | on |
| Stack watermark | on |
| Watchdog timeout | 10 s |

Priority names (higher number = higher priority):

| Name | Value | Typical use |
|---|---|---|
| `MK_PRIO_IDLE` | 1 | idle |
| `MK_PRIO_STORAGE` | 2 | NVS / files |
| `MK_PRIO_DIAGNOSTICS` | 3 | SysMon / health |
| `MK_PRIO_TIME` | 4 | SNTP |
| `MK_PRIO_CONNECTIVITY` | 5 | Wi-Fi supervisor |
| `MK_PRIO_COMMAND` | 6 | CLI |
| `MK_PRIO_GUI` | 7 | display / LVGL |

On the ESP32-S3 port, Nexos-RT task priority `N` is mapped to vendor-runtime priority `8 + N` (GUI 7 → 15), capped at the platform maximum.

Stacks (bytes, ESP-IDF `xTaskCreate` units):

| Macro | Size |
|---|---|
| `MK_STACK_GUI` | 12288 |
| `MK_STACK_COMMAND` | 8192 |
| `MK_STACK_CONNECTIVITY` | 8192 |
| others | 4096 |

Arduino bring-up currently uses 8192 / 4096 / 4096 for GUI / SysMon / CLI.

### 8.4 API surface (application includes `mk.h` only)

**Kernel**

- `mk_init(const mk_config_t*)`
- `mk_start()`
- `mk_yield()` / `mk_sleep_ms(ms)`
- `mk_time_ms()` / `mk_time_us()`
- `mk_is_initialized()` / `mk_is_running()`
- `mk_kernel_get_stats()`

**Tasks**

- `mk_task_create` / `mk_task_create_ext`
- `mk_task_delete` / `mk_task_suspend` / `mk_task_resume`
- `mk_task_self` / `mk_task_get_name` / `mk_task_get_state` / `mk_task_get_info`
- `mk_task_count`

Self-delete of a Next OS task must not double-free the TCB. Entry and argument are stored on the TCB through `stack_pointer` / `stack_base` until a dedicated field is added.

**IPC**

- Mutex: create / lock / unlock / delete
- Semaphore: create / take / give
- Queue: send / receive / send_isr / peek
- Event group: set / clear / wait

**Time / memory / diagnostics**

- Software timers, monotonic clock
- Fixed block pools (`mk_mem_pool_*`) plus `mk_malloc` / `mk_free`
- Stack watermark, CPU load helper, watchdog register/feed

### 8.5 Scheduler object

`mk_scheduler.c` keeps per-priority ready lists and a context-switch counter. The lists are the Next OS scheduler model so a native Xtensa port (`mk_context.S`) can use the same TCB layout.

### 8.6 ESP32-S3 port (not a second OS)

The Espressif Arduino / ESP-IDF package on this chip still supplies Wi-Fi, BLE, USB, and flash drivers. Nexos-RT is the only OS the **application** is written to. That vendor package is **not** offered as a product kernel, is **not** named on the dashboard, and is **not** a `switch_kernel` target.

A native Next OS context-switch port (`arch/esp32s3/mk_context.S`) is the v1.0 goal. Until then, Next OS services still ride the chip support package internally. That is an implementation detail of the ESP32-S3 port, not a dual-OS product.

### 8.7 Roadmap

| Version | Intent |
|---|---|
| v1.2.0 (now) | Nexos-RT API: tasks, PI mutex, IPC, timers, pools, stats |
| v0.9 | 72 h stress, fault injection, deadlock detect |
| v1.0 | Freeze API, native Xtensa `mk_context.S` (window spill, PS/SAR, interrupt vs task stack) |

---

## 9. Two firmware tracks

| | Arduino bring-up (on the chip) | IDF product |
|---|---|---|
| Env | `esp32s3_arduino` (PlatformIO default) | `esp32s3_idf` / `idf.py` |
| Entry | `main/arduino_main.cpp` includes `src/kernel_manager.cpp` + `src/main.cpp` | `main/app_main.cpp` |
| Display | Adafruit GC9A01A hardware SPI at 2 MHz | `esp_lcd` + `esp_lcd_gc9a01` + LVGL 9.5 |
| GUI | GFX dashboard, 1 Hz refresh | `lvgl_adapter` flush → `esp_lcd_panel_draw_bitmap` |
| Commands | small Serial parser | `CommandService` (UART / USB CDC / BLE / Wi-Fi) |
| Wi-Fi / BLE / OTA | not in the Arduino image | full services |
| OS | **Nexos-RT** only | **Nexos-RT** only |

Arduino is the currently flashed bring-up path. Its runtime is verified over COM5; the corrected 3.3 V seven-wire display path must pass the startup red/green/blue test before the dashboard is considered hardware-verified.

---

## 10. Boot sequence

### 13.1 Arduino (current flash)

```
POWER ON
  ROM → SPI_FAST_FLASH_BOOT
  PSRAM enabled
  setup()
    Serial 115200
    Nexos-RT init()           → mk_init v1.2.0
    display mutex
    bind FSPI SCLK=12 MOSI=11
    tft.begin() hardware SPI 2 MHz, CS=9, RST=14
    red → green → blue → black panel test
    startTasks()
      mk GUI   core 1
      mk SysMon core 0
      mk CLI    core 0
      mk_start()
    "SYSTEM READY"
  GUI: branded splash → dashboard
  loop() idles; work is in Nexos-RT tasks
```

### 13.2 IDF product

```
app_main
  mk_init (Next OS)
  platform, event loop, netif
  EventBus, NVS/settings (non-fatal)
  Board + SPI HAL + I2C HAL + USB console
  GC9A01 init
  LVGL adapter
  Time / Diagnostics / OTA (non-fatal)
  spawn GUI / SYSTEM / COMMAND (+ WIFI if enabled)
  mk_start
```

NVS / time / diagnostics / OTA failures must not abort before the first frame (that previously left the panel in post-reset scan).

---

## 11. Tasks, priorities, cores

### Arduino (Next OS, running)

| Next OS name | Entry | mk prio | Core | Stack |
|---|---|---|---|---|
| GUI | `gui_task` | 7 | 1 | 8192 |
| SYSTEM | `sys_monitor_task` | 3 | 1 | 4096 |
| CLI | `cli_task` | 6 | 1 | 4096 |

- GUI: color bars once, then dashboard every 1 s (Next OS chip, heap, uptime)
- SysMon: publishes atomic uptime seconds and logs every 5 s
- CLI: Serial line parser

### IDF product (when built)

| Name | mk prio | Core |
|---|---|---|
| GUI | 7 | 1 |
| SYSTEM | 3 | 1 |
| COMMAND | 6 | 1 |
| WIFI_CONN | 5 | 0 |

GUI loop: `lv_timer_handler` then `mk_sleep_ms` with a **minimum 5 ms** delay so IDLE1 is not starved (task WDT).

---

## 12. Display pipeline

### Arduino

```
gui_task
  Adafruit_GC9A01A hardware FSPI at 2 MHz
    GPIO12 CLK, GPIO11 MOSI, GPIO10 DC
    GPIO9 CS, GPIO14 RST
  fillScreen / drawCircle / print
  invertDisplay(true)  (this GC9A01 module)
```

The ESP32-S3 Arduino variant's native FSPI defaults are 12/11/13/10. The firmware explicitly calls `SPI.begin(12, -1, 11, -1)` so the mapping remains deterministic.

### IDF

```
SpiHal (SPI2 / FSPI)
  → esp_lcd panel IO
    → esp_lcd_gc9a01
      → LvglRuntime.flush_cb
        → esp_lcd_panel_draw_bitmap
```

Panel is write-only; there is no readable chip ID. Success means ESP-side transactions completed.

---

## 13. Source layout

```
smart_device_firmware/
  platformio.ini              Arduino default + IDF env
  CMakeLists.txt              IDF project
  VERSION                     1.0.0
  src/
    main.cpp                  Arduino dashboard + CLI
    kernel_manager.cpp/.h     Next OS bring-up (legacy switch code not product)
  main/
    arduino_main.cpp          includes src/* for PIO src_dir=main
    app_main.cpp              IDF product
  components/
    microkernel/              Next OS (`mk_*` API)
    board/                    pins + BoardConfig
    device_hal/               SPI / I2C / GPIO / USB
    display/                  GC9A01 IDF driver
    lvgl_adapter/             LVGL 9.5, no esp_lvgl_port
    gui/                      dashboard / status / commands
    command/                  unified CommandService
    connectivity/             Wi-Fi + NimBLE
    storage/                  NVS + settings
    time_service/             SNTP
    diagnostics/              health / self-test
    ota/                      dual-slot OTA
    event_bus/                typed events
    platform/                 ESP32-S3 platform shim
    common/                   version, Result<>
  tools/
    pio_kernel_sources.py     compile mk into Arduino
    flash.ps1 / build.ps1 / monitor.ps1
  docs/                       this file and topic docs
  hardware screenshot/        photos + Espressif PDFs
  simulator/                  desktop UI / CLI
```

---

## 14. Serial console

115200 8N1 on the **UART** Micro-USB (CP210x).

### Arduino image (what is on the device)

Product commands (Next OS only):

| Command | Action |
|---|---|
| `help` | list commands |
| `status` | Next OS, heap, task count, uptime |
| `tasks` | Next OS task count + handles |
| `kernel_info` | Next OS block |
| `test` | cyan/green color bars |
| `version` | FW 1.0.0 + pin summary |
| `reboot` | `ESP.restart()` |

`switch_kernel` is **not** a product command. If the current binary still accepts it, that is leftover bring-up code and will be removed in a later firmware drop.

### IDF product (when flashed)

`help`, `version`, `status`, `wifi scan/connect/status`, `ble start/stop/status`, `time status/sync`, `display test`, `display brightness 0-100`, `self-test`, `kernel status/tasks/stats`, `ota status/update`, `reboot`, `factory_reset`.

All IDF transports (UART, USB CDC, BLE, Wi-Fi) share `CommandService`.

---

## 15. Build, flash, monitor

### Arduino (this PC)

```powershell
pio run -e esp32s3_arduino
pio run -e esp32s3_arduino -t upload --upload-port COM5
pio device monitor -p COM5 -b 115200
```

Do not toggle DTR/RTS into download after flash (`boot:0x0 DOWNLOAD`). Open the monitor, or reset with GPIO0 high.

Expected product boot (after the next firmware drop that matches this spec):

```
SMART DEVICE — Next OS
[KERNEL] Active: Next OS
[KERNEL] mk_init v0.8.0
[TASK] GUI on core 1 kernel=Next OS
[KERNEL] mk_start — 3 Next OS tasks
SYSTEM READY. Type help.
```

Glass: red → green → cyan, then dashboard with **Next OS** chip.

### IDF

```powershell
idf.py set-target esp32s3
powershell -ExecutionPolicy Bypass -File tools/build.ps1
powershell -ExecutionPolicy Bypass -File tools/flash.ps1 -Monitor
```

---

## 16. Official documents

Local copies:

| File | What it is |
|---|---|
| `hardware screenshot/esp-dev-kits-en-master-esp32s3.pdf` | **Board** guide. Ch.1 = DevKitC-1 v1.1, Header Block J1/J3, RGB=GPIO38, 5V = J1-21 |
| `hardware screenshot/esp32-s3_datasheet_en.pdf` | **Chip** datasheet v2.2. GPIO 3.3 V, FSPI IOMUX, no 5 V GPIO, octal PSRAM, power-up GPIO glitch |
| `hardware screenshot/ESP32-S3_DevKitC-1_pinlayout_v1.1.jpg` | Official pin-layout art (USB at bottom) |

Web:

- DevKitC-1 v1.1 user guide: https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-devkitc-1/user_guide_v1.1.html
- Schematic: https://dl.espressif.com/dl/schematics/SCH_ESP32-S3-DevKitC-1_V1.1_20221130.pdf
- ESP32-S3 series datasheet: https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf

Repo topic docs: `docs/architecture.md`, `docs/architecture_microkernel.md` (Nexos-RT), `docs/hardware.md`, `docs/commands.md`, `docs/ota.md`, `docs/device_mapping_and_state.md`.

---

## 17. Known pitfalls

1. **TFT VCC on 5 V.** This TFT VER1.0 module is specified for 3.3 V and may be damaged by 5 V.
2. **RST floating.** Connect RST directly to GPIO14.
3. **CS floating.** Connect CS directly to GPIO9.
4. **SCL/SDA treated as I2C.** They are SPI SCLK/MOSI on GPIO12/11.
5. **J3 silk `21` is not a supply pin.**
6. **SPI signal wires one breadboard row off.**
7. **Octal PSRAM GPIO35–37** must stay unused.
8. **Download-mode serial open.** RTS/DTR can leave `boot:0x0 DOWNLOAD(USB/UART0)`. Reset with IO0 high.
9. **Wrong breadboard strip.** Each ESP pin is only common with its own 5-hole row. One row off → snow or blank glass while the MCU still runs.
10. **IDF GUI prio 15 with 1 ms LVGL delay** starves IDLE1 / task WDT. Floor delay at 5 ms.

---

## Quick card (print this)

```
ESP32-S3-DevKitC-1 v1.1  WROOM-1 N8R8
Display 1.28" GC9A01 SPI  (SCL/SDA = CLK/MOSI)
OS: Nexos-RT v1.2.0  (only product kernel)

USB-end, bottom row, from UART:
  G  → GND
  5V empty
  14 → RST    13 empty
  12 → SCL
  11 → SDA
  10 → DC
  9  → CS
  3V3 → VCC  (antenna-end J1 supply)

Flash:   pio run -e esp32s3_arduino -t upload --upload-port COM5
```

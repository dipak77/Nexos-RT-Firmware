# Smart Device Firmware — Complete System Reference

**Status:** live on hardware as of 2026-08-30  
**Firmware version:** 1.0.0  
**Product OS:** **Next OS** v0.8.0 (`mk_*` API) — the only application operating system  
**Active build:** PlatformIO `[env:esp32s3_arduino]`  
**UART:** Silicon Labs CP210x, **COM5**, 115200 baud  
**MAC (this board):** `1c:db:d4:9c:1d:78`

This file is the canonical description of the project: hardware, proven wiring, **Next OS**, boot flow, pin map, commands, and source layout. Older notes in `docs/hardware.md` and `README.md` that wire **CS → GPIO9** and **RST → GPIO14** are **superseded**. That RST mapping blanks this panel.

**Product policy:** Next OS is the only OS name. Dual-kernel switch language is withdrawn.

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
| Product OS | **Next OS** v0.8.0 — only application OS |
| Product GUI (IDF track) | LVGL 9.5 + glassmorphism dashboard |
| Bring-up GUI (Arduino track, **what is flashed**) | Adafruit GC9A01A **software SPI** dashboard |

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
- Chinese note at R8: **R8 is a CS pull-down; this module may leave CS and RST unconnected**
- **No BLK pin.** Backlight LED is tied to VCC. Feed **5 V** or the glass stays dim/dark while the MCU still runs.

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
| 9 | FSPIHD | leave open (CS) |
| 14 | FSPIWP | leave open (RST) |

Power options (mutually exclusive): USB-to-UART and/or ESP USB; **5V + G**; or **3V3 + G**.

---

## 4. Working display wiring (proven)

This is the map that produced pixels after the USB-end rewire. Do not use the antenna-end `3V3` cluster.

### 4.1 Desk orientation (same as the working photo)

- Metal can / antenna = **left**
- Two USB sockets = **right**
- Display sits **below** the board
- Use the **bottom** row of pins (J1). That row has `3V3` at the antenna and `5V` `G` at the UART USB.

Do **not** use the top row (`TX` `RX` `21`). Silk `21` there is GPIO21.

### 4.2 Count from the UART USB, bottom row

Start at the hole **closest to the UART socket** and walk toward the antenna:

| Count from UART | Silk | Display wire |
|---|---|---|
| 1 | **G** | **GND** |
| 2 | **5V** | **VCC** |
| 3 | **14** | **empty** |
| 4 | **13** | **empty** |
| 5 | **12** | **SCL** |
| 6 | **11** | **SDA** |
| 7 | **10** | **DC** |
| 8 | **9** | **empty** |

```
antenna (left)                                    UART USB (right)
3V3 …  9   10   11   12   13   14   5V   G
           │    │    │                   │    │
           DC   SDA  SCL                 VCC  GND
```

### 4.3 Round PCB — five wires only

| Display silk | ESP silk | Notes |
|---|---|---|
| **VCC** | **5V** | Backlight is on VCC. `3V3` looks like a dim blue disc. |
| **GND** | **G** | The `G` next to `5V`, USB corner. |
| **SCL** | **12** | SPI clock |
| **SDA** | **11** | SPI MOSI. Not I2C. |
| **DC** | **10** | Command / data |
| **CS** | **no wire** | R8 holds CS low. Driving GPIO9 HIGH deselects the panel. |
| **RST** | **no wire** | GPIO14 next to 5V/G; driving it holds the GC9A01 in reset (black glass). |

### 4.4 Breadboard rule

The DevKit sits in the breadboard. Each ESP pin is common only with the **other holes in that same 5-hole row**.

- Plug each Dupont in the **same row** as the counted pin.
- Do not use the long red/blue power rails unless you have verified they are tied to `5V`/`G`.
- One row off = firmware still runs (RGB LED / serial alive), glass stays flat blue.

Unplug USB before moving 5 V. Do not hot-swap VCC.

---

## 5. Pin mapping (firmware vs silk)

### 5.1 Arduino track (flashed, working)

File: `src/main.cpp`

| Signal | Define | Value |
|---|---|---|
| MOSI | `TFT_MOSI` | 11 |
| SCLK | `TFT_SCLK` | 12 |
| DC | `TFT_DC` | 10 |
| CS | `TFT_CS` | **-1** (not driven) |
| RST | `TFT_RST` | **-1** (not driven) |
| GPIO9 | `pinMode(9, INPUT)` | high-Z |
| GPIO14 | `pinMode(14, INPUT)` | high-Z |

Driver: `Adafruit_GC9A01A tft(CS, DC, MOSI, SCLK, RST, MISO)` — **software SPI**.  
The hardware-SPI constructor `Adafruit_GC9A01A(&SPI, dc, cs, rst)` re-inits SPI to ESP32-S3 default pins and blanks this panel.

### 5.2 IDF product track (not the image currently on the chip)

File: `components/board/include/board_pins.h`

| Signal | Define | Value |
|---|---|---|
| MOSI | `LCD_PIN_MOSI` | 11 |
| SCLK | `LCD_PIN_SCLK` | 12 |
| DC | `LCD_PIN_DC` | 10 |
| CS | `LCD_PIN_CS` | 9 (driver still toggles CS if this stays 9) |
| RST | `LCD_PIN_RST` | -1 |
| BL | `LCD_PIN_BL` | -1 |
| Pixel clock | `LCD_PIXEL_CLOCK_HZ` | 2 MHz |
| I2C expansion SDA/SCL | `I2C_EXP_SDA` / `I2C_EXP_SCL` | 16 / 17 (**not** the LCD) |

Hardware on the desk must still leave **CS and RST unplugged** even if the IDF driver drives GPIO9: R8 already selects the chip. Do not jumper RST to 14.

---

## 6. Why this wiring is required


| Symptom | Cause | Fix |
|---|---|---|
| Dim / flat blue glass, MCU LED on | VCC on J1 `3V3` (antenna end) | Move VCC to USB-end silk `5V` |
| Completely black | LCD RST on GPIO14 (active-low, glitches low at power-up ~60 µs, or held low) | Leave RST open; firmware `RST = -1` |
| Backlight on, no pixels / snow | Duponts on antenna-end J1 (3V3/RST/GPIO4…) while firmware bit-bangs 10/11/12 | Move SCL/SDA/DC to silk 12/11/10 next to 5V/G |
| Panel selected then dropped | Firmware driving GPIO9 as CS HIGH | `TFT_CS = -1`, CS header open |
| Adafruit HW SPI, blank | `Adafruit_GC9A01A(&SPI, …)` remaps pins | Software-SPI constructor |
| GPIO21 used as “5V” | J3 silk `21` is GPIO21 | Only J1 silk `5V` is 5 V |

GPIO9–14 are **not** strapping pins. They **do** have a short low glitch at power-up (chip datasheet Table 2-2). That is lethal if RST is tied to GPIO14.

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
|  Next OS  v0.8.0                                                 |
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
            Next OS (mk_*) + ESP-IDF chip support
```

Core split:

- **Core 0:** chip support (Wi-Fi, BLE, lwIP), SysMon, CLI
- **Core 1:** GUI (Next OS affinity 1)

There is no second product kernel. Application tasks are created only with `mk_task_create_ext`.

---

## 8. Next OS

**Name:** Next OS  
**API prefix:** `mk_*`  
**Version:** 0.8.0 (`MK_CONFIG_VERSION_STRING`)  
**Source:** `components/microkernel/`  
**Dashboard chip:** **Next OS** (the former “Microkernel” label)

### 8.1 What it is

Next OS is the product operating system: a C API (`mk_*`) plus C++ RAII (`mk_cpp.hpp`). Application code includes `mk.h` only. Tasks, sleep, IPC, timers, and pools all go through this API.

Product policy: **Next OS is the only OS.** There is no user-facing kernel switch.

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
| Version | 0.8.0 |
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

On the ESP32-S3 port, Next OS task priority `N` is mapped to vendor-runtime priority `8 + N` (GUI 7 → 15), capped at the platform maximum.

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

The Espressif Arduino / ESP-IDF package on this chip still supplies Wi-Fi, BLE, USB, and flash drivers. Next OS is the only OS the **application** is written to. That vendor package is **not** offered as a product kernel, is **not** named on the dashboard, and is **not** a `switch_kernel` target.

A native Next OS context-switch port (`arch/esp32s3/mk_context.S`) is the v1.0 goal. Until then, Next OS services still ride the chip support package internally. That is an implementation detail of the ESP32-S3 port, not a dual-OS product.

### 8.7 Roadmap

| Version | Intent |
|---|---|
| v0.8.0 (now) | Next OS API: tasks, PI mutex, IPC, timers, pools, stats |
| v0.9 | 72 h stress, fault injection, deadlock detect |
| v1.0 | Freeze API, native Xtensa `mk_context.S` (window spill, PS/SAR, interrupt vs task stack) |

---

## 9. Two firmware tracks

| | Arduino bring-up (on the chip) | IDF product |
|---|---|---|
| Env | `esp32s3_arduino` (PlatformIO default) | `esp32s3_idf` / `idf.py` |
| Entry | `main/arduino_main.cpp` includes `src/kernel_manager.cpp` + `src/main.cpp` | `main/app_main.cpp` |
| Display | Adafruit GC9A01A software SPI | `esp_lcd` + `esp_lcd_gc9a01` + LVGL 9.5 |
| GUI | GFX dashboard, 1 Hz refresh | `lvgl_adapter` flush → `esp_lcd_panel_draw_bitmap` |
| Commands | small Serial parser | `CommandService` (UART / USB CDC / BLE / Wi-Fi) |
| Wi-Fi / BLE / OTA | not in the Arduino image | full services |
| OS | **Next OS** only | **Next OS** only |

Arduino is the path that compiled, flashed, and painted the round TFT on this PC. The IDF image is the intended product (LVGL, OTA, connectivity) once that toolchain is used.

---

## 10. Boot sequence

### 13.1 Arduino (current flash)

```
POWER ON
  ROM → SPI_FAST_FLASH_BOOT
  PSRAM enabled
  setup()
    Serial 115200
    Next OS init()            → mk_init v0.8.0
    display mutex
    GPIO9/14 INPUT
    tft.begin() software SPI, rotation 0, invert on
    startTasks()
      mk GUI   core 1
      mk SysMon core 0
      mk CLI    core 0
      mk_start()
    "SYSTEM READY"
  GUI: red → green → cyan → black dashboard
  loop() idles; work is in Next OS tasks
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
| SYSTEM | `sys_monitor_task` | 3 | 0 | 4096 |
| CLI | `cli_task` | 6 | 0 | 4096 |

- GUI: color bars once, then dashboard every 1 s (Next OS chip, heap, uptime)
- SysMon: `g_seconds_counter++`, log every 5 s
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

### Arduino (working)

```
gui_task
  Adafruit_GC9A01A software SPI
    GPIO12 CLK, GPIO11 MOSI, GPIO10 DC
    CS/RST not driven
  fillScreen / drawCircle / print
  invertDisplay(true)  (this GC9A01 module)
```

SPI is bit-banged so the pin numbers in the constructor are the pins that actually toggle. Hardware SPI on this Adafruit port re-binds to S3 defaults and misses 11/12.

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

Repo topic docs: `docs/architecture.md`, `docs/architecture_microkernel.md` (Next OS), `docs/hardware.md` (CS/RST rows there are outdated), `docs/commands.md`, `docs/ota.md`, `docs/device_mapping_and_state.md`.

---

## 17. Known pitfalls

1. **Antenna-end Duponts.** Firmware talks to silk 10/11/12/5V/G. Wires next to the metal can are 3V3/EN/GPIO4.
2. **RST on 14.** Black screen. Leave open.
3. **CS driven high.** Deselects a panel whose R8 already holds CS low. Leave CS open on this module.
4. **VCC on 3V3.** Backlight too weak.
5. **J3 silk `21` ≠ 5 V.**
6. **Adafruit hardware SPI** on ESP32-S3.
7. **Octal PSRAM GPIO35–37** must stay unused.
8. **Download-mode serial open.** RTS/DTR can leave `boot:0x0 DOWNLOAD(USB/UART0)`. Reset with IO0 high.
9. **Wrong breadboard strip.** Each ESP pin is only common with its own 5-hole row. One row off → snow or blank glass while the MCU still runs.
10. **IDF GUI prio 15 with 1 ms LVGL delay** starves IDLE1 / task WDT. Floor delay at 5 ms.

---

## Quick card (print this)

```
ESP32-S3-DevKitC-1 v1.1  WROOM-1 N8R8
Display 1.28" GC9A01 SPI  (SCL/SDA = CLK/MOSI)
OS: Next OS v0.8.0  (only product kernel)

USB-end, bottom row, from UART:
  G  → GND
  5V → VCC
  14 empty    13 empty
  12 → SCL
  11 → SDA
  10 → DC
  CS unplugged    RST unplugged

Flash:   pio run -e esp32s3_arduino -t upload --upload-port COM5
```

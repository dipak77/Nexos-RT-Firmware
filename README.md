# Smart Device Platform - ESP32-S3 + GC9A01 + LVGL 9.5 (Next OS)

Production-grade firmware platform for circular smart displays powered by **ESP32-S3**, **GC9A01 240x240 Round TFT**, **LVGL 9.5.0**, and **Next OS** (`mk_*`). Next OS is the only application operating system.

---

## 🚀 Key Specifications & Stack

- **MCU**: ESP32-S3-WROOM-1 (DevKitC-1 v1.1, Dual-Core Xtensa LX7 @ 240MHz)
- **OS**: **Next OS** v0.8.0 (`mk_*` API, preemptive priority scheduling, PI mutexes, Core 1 application affinity). Only product kernel. Chip Wi-Fi/NimBLE stay on the Espressif support package (Core 0), not as a second OS.
- **Display**: GC9A01 240x240 Round TFT via SPI2 Master (DMA-backed)
- **GUI Engine**: LVGL 9.5.0 with custom `lvgl_adapter::LvglRuntime` and Glassmorphism circular theme
- **Wireless**: Wi-Fi (Station mode, WPA2/WPA3, auto-connect), NimBLE (GAP/GATT advertising & sync)
- **Services**: SNTP Precision Time (IST +5:30 default), NVS Settings Store, HTTPS OTA with Rollback, Diagnostics & Self-Test, Unified Event Bus
- **Console & I/O**: UART0 Console, USB CDC-ACM, I2C Expansion Port

---

## 📐 Hardware Wiring

The GC9A01 is **4-wire SPI**, not I2C. Silk `SCL`/`SDA` on the round PCB are SPI CLK and SPI MOSI. `DC` and `CS` exist only on SPI panels. This 7-pin module has no BLK pin — backlight is tied to VCC, so feed **5V**.

| GC9A01 silk | ESP32-S3 GPIO | Function |
|---|---|---|
| **VCC** | **5V** | Power + backlight (3V3 often looks like “no power”) |
| **GND** | GND | Ground |
| **SCL** | GPIO 12 | SPI CLK (not I2C) |
| **SDA** | GPIO 11 | SPI MOSI (not I2C) |
| **DC** | GPIO 10 | Data/Command |
| **CS** | **unplugged** | R8 pulls CS low. Leave the header open. |
| **RST** | **unplugged** | Do not wire RST to GPIO14 (holds the panel in reset). |

I2C (sensors/touch) is a **separate** bus on GPIO 16/17. Do not wire the LCD there.

> [!NOTE]
> Do NOT assign pins to GPIO 19/20 (native USB D-/D+) or GPIO 43/44 (UART0 console).

---

## 💻 Quick Start Guide

### 1. Desktop Simulation (No Hardware Needed)

Test the 240x240 circular UI and interactive UART command engine right on your PC:

```powershell
# Launch Desktop Visual Simulator (Tkinter + Glassmorphism UI)
powershell -ExecutionPolicy Bypass -File tools/run_simulator.ps1

# Or run Headless CLI shell:
python simulator/run_simulator.py --cli

# Run Automated Unit Tests:
python test/run_tests.py
```

---

### 2. Environment Verification

Check your local prerequisites and connected hardware:

```powershell
powershell -ExecutionPolicy Bypass -File tools/setup_env.ps1
```

---

### 3. Build & Flash (ESP32-S3 Hardware)

From an ESP-IDF PowerShell prompt (or VS Code with ESP-IDF extension):

```powershell
# 1. Set MCU target
idf.py set-target esp32s3

# 2. Build the firmware
powershell -ExecutionPolicy Bypass -File tools/build.ps1

# 3. Flash and monitor (auto-detects COM port or pass -Port COMx)
powershell -ExecutionPolicy Bypass -File tools/flash.ps1 -Monitor
```

---

## ⌨️ Command Console (UART / Shell)

The device exposes an interactive command engine (`device> ` prompt):

| Command | Arguments | Description |
|---|---|---|
| `help` | - | List all available system commands |
| `version` | - | Display FW, Kernel, HW version, and build metadata |
| `status` | - | Full diagnostics (Heap, Uptime, WiFi, BLE, Time, Display) |
| `wifi scan` | - | Scan for nearby WiFi Access Points |
| `wifi connect` | `<ssid> <password>` | Connect to WiFi network and save credentials to NVS |
| `wifi status` | - | Query current WiFi connection state and IP address |
| `ble status` | - | Query NimBLE advertising/connection status |
| `ble start` | - | Start BLE GAP advertising |
| `ble stop` | - | Stop BLE advertising |
| `time status` | - | Display current SNTP clock and timezone |
| `time sync` | - | Trigger immediate NTP clock synchronization |
| `display test` | - | Run GC9A01 LCD color inversion test pattern |
| `display brightness` | `<0-100>` | Adjust backlight brightness level via PWM |
| `self-test` | - | Execute comprehensive 10-point hardware/driver diagnostic suite |
| `kernel status` | - | Display Next OS scheduler stats (tasks, context switches, heap) |
| `kernel tasks` | - | List Next OS task table and priority map |
| `kernel stats` | - | Export Next OS statistics in JSON format |
| `reboot` | - | Perform clean software reset |
| `factory_reset` | - | Reset all NVS settings to factory defaults |

---

## Architecture Overview (Next OS)

```
+-------------------------------------------------------------------+
|               Product Application & Glassmorphism GUI             |
|                   (Next OS mk_* C / C++ RAII API Only)            |
+-------------------------------------------------------------------+
                                  |
                                  v
+-------------------------------------------------------------------+
|                            Next OS                                |
|   (Scheduler, Preemptive Tasks, PI Mutex, Queue, Timers, Pools)   |
+-------------------------------------------------------------------+
                                  |
               +------------------+------------------+
               | (Core 1)                            | (Core 0)
               v                                     v
+-----------------------------+       +-----------------------------+
|    Application Core Tasks   |       |   Chip support & drivers    |
| - GUI Thread (Prio 7)       |       | - ESP-IDF WiFi & LWIP Stack |
| - Command Engine (Prio 6)   |       | - NimBLE Bluetooth Stack    |
| - System Monitor (Prio 3)   |       | - SPI2 DMA GC9A01 Driver    |
+-----------------------------+       +-----------------------------+
                                  |
                                  v
                      ESP32-S3 Hardware Layer
```

Canonical wiring, pin map, and Next OS detail: `docs/COMPLETE_SYSTEM_REFERENCE.md`.

---

## 📋 Production Checklist V1.0.0

- [x] Board bring-up & hardware initialization
- [x] GC9A01 240x240 circular LCD + direct LVGL 9.5 adapter
- [x] Next OS v0.8.0 with Priority Inheritance mutexes
- [x] Glassmorphism circular dashboard with boot animations & status chips
- [x] Real-time command feedback card with PASS/FAIL indicators
- [x] WiFi (auto-connect, scan, status) + NimBLE BLE service
- [x] SNTP time synchronization (IST-5:30)
- [x] NVS settings persistence & factory reset
- [x] OTA HTTPS update service & partition rollback
- [x] Desktop cross-platform visual simulator & test suite

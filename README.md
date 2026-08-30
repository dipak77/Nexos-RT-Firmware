# Nexos-RT Smart Device Firmware — ESP32-S3 + GC9A01 1.28" Round Display

Production-grade embedded firmware platform for circular smart displays powered by **ESP32-S3**, **GC9A01 240x240 Round TFT**, and **Nexos-RT v1.1** standalone real-time runtime microkernel (`mk_*`).

[![Firmware Build](https://img.shields.io/badge/Nexos--RT-v1.1%20Hardened-brightgreen.svg)](https://github.com/dipak77/Nexos-RT-Firmware)
[![Platform](https://img.shields.io/badge/Platform-ESP32--S3-blue.svg)](https://www.espressif.com/)
[![Display](https://img.shields.io/badge/Display-GC9A01%20240x240-orange.svg)](https://github.com/dipak77/Nexos-RT-Firmware)
[![Tests](https://img.shields.io/badge/Tests-10%2F10%20PASS-success.svg)](https://github.com/dipak77/Nexos-RT-Firmware)

---

## 🚀 Key Specifications & Stack

- **MCU**: ESP32-S3-WROOM-1 (DevKitC-1 v1.1, Dual-Core Xtensa LX7 @ 240MHz, 8MB Flash, 8MB Octal PSRAM, 342KB internal SRAM)
- **Operating System**: **Nexos-RT v1.1** (`mk_*` API, preemptive priority scheduling, PI mutexes, start gate synchronization, multi-core spinlock protection, and 4-second watchdog monitors).
- **Display**: GC9A01 240x240 Round TFT via SPI (Adafruit GFX / LVGL 9.5.0 direct adapter with glassmorphic circular dashboard & Nexos-RT geometric boot visualizer).
- **Wireless**: Wi-Fi Station (WPA2/WPA3, auto-connect), Apache NimBLE (GAP/GATT hydration & telemetry profiles).
- **Core Services**: SNTP Precision Time (IST +5:30 default), NVS Settings Store, Dual-bank OTA with Rollback, Diagnostics & Watchdog Stall Detection, Unified Event Bus.
- **Smart Hydration & Sensor Ready**: I2C expansion bus on GPIO 16/17 for 6-DOF IMU (sip tracking), temperature sensors (water thermal warning), and fuel gauge IC.

---

## 📐 Hardware Wiring & Pin Mapping

The GC9A01 is **4-wire SPI**, not I2C. Silk `SCL`/`SDA` on the round PCB are SPI CLK and SPI MOSI. `DC` and `CS` exist only on SPI panels. This 7-pin module has no BLK pin — backlight is tied to VCC, so feed **5V**.

| GC9A01 silk | ESP32-S3 Header J1 | GPIO | Function & Wiring Notes |
|---|---|---|---|
| **VCC** | **J1-21** | **5V** | Power + Backlight (tie to 5V; 3V3 looks like “no power”) |
| **GND** | **J1-22** | **GND** | Ground |
| **SCL** | **J1-18** | **GPIO 12** | SPI Clock (Software SPI / Native FSPI) |
| **SDA** | **J1-17** | **GPIO 11** | SPI MOSI (Software SPI / Native FSPI) |
| **DC** | **J1-16** | **GPIO 10** | Data / Command |
| **CS** | *Unconnected* | *Open* | Resistor `R8` on display pulls CS low permanently |
| **RST** | *Unconnected* | *Open* | Leave open (prevents GPIO14 boot glitch from holding panel in reset) |

> [!IMPORTANT]
> **5-Wire Direct Plug-in:** Only plug 5 adjacent pins on header J1 (pins 16, 17, 18, 21, 22). Leave CS and RST unplugged.

---

## 💻 Quick Start & Build

### 1. Build Firmware via PlatformIO
```powershell
# Build Nexos-RT Arduino bring-up target
pio run -e esp32s3_arduino

# Flash to device (auto-detects COM port)
pio run -e esp32s3_arduino -t upload

# Open serial monitor @ 115200 baud
pio device monitor -b 115200
```

### 2. Desktop Simulation & Unit Tests
```powershell
# Run Automated Python Unit Tests (10/10 PASS)
python test/run_tests.py

# Launch Desktop Visual Simulator
python simulator/run_simulator.py --cli
```

---

## ⌨️ Nexos-RT Interactive CLI Console

The device exposes an interactive non-blocking command shell on UART0 (`115200 baud`):

| Command | Description |
|---|---|
| `help` | List available system commands (`status`, `tasks`, `kernel_info`, `health`, `boot`, `test`, `version`, `reboot`) |
| `status` / `health` | Report OS name, version, health status (`[OK] OK`), free heap (KB), tasks, and uptime |
| `tasks` | Display active Nexos-RT tasks, task control block pointers, and priority levels |
| `kernel_info` | Print full kernel telemetry banner (CPU cores, preemption state, watchdog ages, SRAM/PSRAM metrics) |
| `boot` | Replay high-definition Nexos-RT branding boot flash and 5-stage progressive loading sequence |
| `test` | Trigger dynamic display sweep color diagnostic sequence |
| `version` | Display firmware, kernel, and hardware wiring specification |
| `reboot` | Perform clean software reset |

---

## 🏛️ System Architecture

```
+---------------------------------------------------------------------------------------+
|                       APPLICATION & PRODUCT UI LAYER                                  |
|   src/main.cpp  |  src/kernel_manager.cpp  |  components/gui/  |  components/command/ |
|                                                                                       |
|   • Includes mk.h ONLY (Zero vendor FreeRTOS symbols in application code)              |
|   • Thread-safe UI rendering via DisplayGuard RAII & Display Mutex                     |
|   • Heartbeat feeding on GUI, SYSTEM, and CLI tasks                                   |
+---------------------------------------------------------------------------------------+
                                           |
                                           | #error barrier if INC_FREERTOS_H included
                                           v
+---------------------------------------------------------------------------------------+
|                             NEXOS-RT CORE & PUBLIC API                                |
|   components/microkernel/include/mk.h  |  mk_task.h  |  mk_mutex.h  |  mk_kernel.h    |
|   components/microkernel/core/mk_kernel.c  |  mk_task.c  |  mk_diagnostics.c          |
|                                                                                       |
|   • Slot State Machine (SLOT_FREE, SLOT_RESERVED, SLOT_LIVE, SLOT_DELETING)           |
|   • Launch Gate EventGroup (s_start_gate) & Task Spawn Rollback                       |
|   • Multi-Core SMP Spinlocks (s_task_lock) & Thread-Safe Memory Pool (mk_pool.c)      |
|   • 4-Second Watchdog Subsystem & SRAM-Only Heap Accounting (342 KB)                  |
+---------------------------------------------------------------------------------------+
                                           |
                                           | Quarantined internal chip port header
                                           v
+---------------------------------------------------------------------------------------+
|                        ESP32-S3 CHIP SUPPORT PORT (INTERNAL)                          |
|   components/microkernel/arch/esp32s3/mk_chip_port.h                                  |
|   components/microkernel/arch/esp32s3/mk_port_shim.c                                  |
|                                                                                       |
|   • Core 1: Dedicated to Nexos-RT Application Tasks (GUI, SYSTEM, CLI)                |
|   • Core 0: Dedicated to Radio & Protocol Stacks (Wi-Fi Station, NimBLE, LwIP)        |
+---------------------------------------------------------------------------------------+
                                           |
                                           v
+---------------------------------------------------------------------------------------+
|                                ESP32-S3 HARDWARE                                      |
|   Dual Xtensa LX7 @ 240 MHz  |  342 KB SRAM (Internal)  |  8189 KB PSRAM (Octal)      |
+---------------------------------------------------------------------------------------+
```

---

## 🍼 Smart Hydration & Water Bottle Ready

Nexos-RT is architected for smart cap hydration devices with 1.28" round screens:
- **I2C Expansion (GPIO 16/17)**: Connects 6-DOF IMU for sip tilt tracking ($V = Q_{\text{flow}} \times t_{\text{sip}} \times \sin\theta$) and water temperature sensor.
- **Radial UI Arc**: 360° intake progress ring, cold/normal/hot water temperature warning colors.
- **UV-C Safety Interlock**: UV-C LED self-cleaning cycle with magnetic cap switch interlock.
- **Ultra-Low Power**: Display sleep (`SLPIN`) and wake-on-tilt deep sleep.

---

## 📄 Canonical Documentation Reference

- [COMPLETE_SYSTEM_REFERENCE.md](file:///c:/Users/haran/source/repos/smart_device_firmware/docs/COMPLETE_SYSTEM_REFERENCE.md) — Definitive single source of truth for wiring, pinouts, and architecture.
- [Nexos-RT Production Hardening Plan](file:///c:/Users/haran/source/repos/smart_device_firmware/docs/Nexos-RT-Production-Hardening-Plan.md) — Detailed C1–C18 resolution audit.
- [BUILD_INSTRUCTIONS.md](file:///c:/Users/haran/source/repos/smart_device_firmware/BUILD_INSTRUCTIONS.md) — Comprehensive compilation and flashing instructions.


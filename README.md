# Nexos-RT Smart Device Firmware

ESP32-S3 + 1.28" GC9A01 round TFT firmware. The product OS is **Nexos-RT V1.2** (`mk_*`).

**Live on hardware:** PlatformIO `[env:esp32s3_arduino]`, UART **COM5** (CP210x), MAC `1c:db:d4:9c:1d:78`.

| | |
|---|---|
| Firmware | 1.2.0 |
| OS | **Nexos-RT V1.2.0** |
| Board | ESP32-S3-DevKitC-1 **v1.1**, WROOM-1 **N8R8** |
| Display | 1.28" TFT VER1.0, GC9A01, 240×240, 4-wire SPI |
| CPU | Dual Xtensa LX7 @ 240 MHz |
| Memory | 8 MB Quad flash, 8 MB Octal PSRAM, ~342 KB internal SRAM |

Nexos-RT is the **only** application OS. There is no dual-kernel switch. Application code uses `mk.h` / `mk_*`. The ESP32-S3 vendor package (Arduino-ESP32 / ESP-IDF) still provides Wi-Fi, BLE, USB, and flash drivers through a quarantined chip port (`mk_chip_port.h`). That package is **not** a second product kernel.

---

## What is on the glass today

The image flashed to this board is the **Arduino bring-up**:

1. Power on (UART USB)
2. **NEXOS-RT** branded splash (rings, N mark, **BASE OS V1.2**, loading: KERNEL CORE → SCHEDULER → DRIVERS → DISPLAY → READY)
3. Dashboard chip **Nexos-RT**, health, heap, uptime

Replay splash from serial: `boot` (GUI task owns the TFT; CLI only queues the splash).

---

## Hardware wiring (proven)

Hold the DevKit **antenna / metal can left, USB ports right**. Use the **bottom** header (J1). Count from the **UART** USB toward the antenna.

| Count from UART | Silk | Display silk | Wire? |
|---|---|---|---|
| 1 | **G** | GND | yes |
| 2 | **5V** | — | **empty — do not power this TFT from 5 V** |
| 3 | **14** | RST | yes |
| 4 | 13 | — | **empty** |
| 5 | **12** | SCL (SPI CLK) | yes |
| 6 | **11** | SDA (SPI MOSI, not I2C) | yes |
| 7 | **10** | DC | yes |
| 8 | **9** | CS | yes |
| 21 or 22 | **3V3** | VCC | yes — TFT VER1.0 is a 3.3 V module |

Connect **CS → GPIO9** and **RST → GPIO14**. The firmware pulses reset and toggles CS for each hardware-SPI transaction; this removes reliance on optional board resistors and makes warm reboots deterministic.

Display header order: `VCC GND SCL SDA DC CS RST`. No BLK pin.

**Breadboard:** each ESP pin is common only with the other holes in that **same 5-hole row**. One row off → snow or blank glass while the MCU still runs.

**Do not use**

| GPIO / silk | Why |
|---|---|
| J1 `5V` (USB end) | Over-volts the 3.3 V TFT module |
| J3 silk `21` | GPIO21, **not** a power pin |
| GPIO 35 / 36 / 37 | Octal PSRAM inside N8R8 |
| GPIO 19 / 20 | Native USB |
| GPIO 43 / 44 | UART0 console |
| GPIO 38 | RGB LED |

The DevKit does expose USB **5V**, but it is not the TFT supply. Use either J1 **3V3** pin for display VCC. Board header reference: `hardware screenshot/esp-dev-kits-en-master-esp32s3.pdf` Chapter 1.

---

## Firmware pin map

**Arduino (flashed, working)** — `src/main.cpp`

| Signal | Define | Value |
|---|---|---|
| MOSI | `TFT_MOSI` | 11 |
| SCLK | `TFT_SCLK` | 12 |
| DC | `TFT_DC` | 10 |
| CS | `TFT_CS` | **9** |
| RST | `TFT_RST` | **14** |

Driver: `Adafruit_GC9A01A` hardware FSPI at a conservative **2 MHz**. The firmware explicitly binds SCLK 12 and MOSI 11 before the driver initializes.

**IDF product track** — `components/board/include/board_pins.h`  
Same MOSI/CLK/DC/CS/RST pin map. I2C expansion (not LCD): GPIO 16 / 17.

---

## Two firmware tracks

| | Arduino bring-up (on the chip) | IDF product |
|---|---|---|
| Env | `esp32s3_arduino` (default) | `esp32s3_idf` / `idf.py` |
| Entry | `main/arduino_main.cpp` → `src/main.cpp` + `src/kernel_manager.cpp` | `main/app_main.cpp` |
| Display | Adafruit GFX hardware SPI, 2 MHz | `esp_lcd` + GC9A01 + LVGL 9.5 |
| GUI | Round dashboard + splash | LVGL dashboard |
| Console | UART commands below | `CommandService` (UART / USB / BLE / Wi-Fi) |
| Wi-Fi / BLE / OTA | not in this image | full services |
| OS | Nexos-RT only | Nexos-RT only |

---

## Nexos-RT architecture

```
Application (src/main.cpp, GUI, command, services)
        |  include mk.h only
        v
Nexos-RT  mk_*   (components/microkernel/)
  mk_init → create tasks (they wait) → mk_start() opens launch gate
  mutex, queue, event, timer, pool, watchdog
        |
        v
ESP32-S3 chip port  (arch/esp32s3/mk_chip_port.h)  — internal
        |
        v
ESP-IDF / Arduino-ESP32 drivers (Wi-Fi, BLE, USB, flash)
        |
        v
ESP32-S3-WROOM-1-N8R8 + GC9A01
```

### Boot (Arduino)

```
POWER ON → ROM SPI boot → PSRAM
  KernelManager::init()     mk_init + diagnostics + display mutex
  SPI.begin(12, -1, 11, -1) + tft.begin(2 MHz hardware SPI)
  NEXOS-RT splash
  spawn GUI / SYSTEM / CLI on core 1 (waiting on start gate)
  mk_start()                gate opens
  dashboard
```

### Tasks (Arduino image)

| Name | `mk` prio | Port prio | Core | Stack | Watchdog |
|---|---|---|---|---|---|
| GUI | 7 | 16 | 1 | 8192 | 4 s |
| SYSTEM | 3 | 12 | 1 | 4096 | 4 s |
| CLI | 6 | 5 | 1 | 4096 | 4 s |

Core 0: vendor radio / USB. App tasks stay on core 1.

### Kernel mechanics (V1.2 hardening)

- Launch gate: tasks block until `mk_start()` (no run-before-register race)
- Task slots: FREE → RESERVED → LIVE → DELETING, SMP spinlock
- Real `entry` / `arg` fields (not stuffed into stack pointers)
- Spawn rollback if a required task fails
- `DisplayGuard` RAII, finite lock timeout
- Non-blocking CLI (byte machine, not `readStringUntil`)
- Uptime from `mk_time_ms()/1000`
- Watchdog `mk_watchdog_feed_self()`, health text copied into a buffer
- Heap stats use **internal SRAM** only (PSRAM must not hide SRAM exhaustion)

Nexos-RT is **Architecture A**: a premium runtime over the ESP32-S3 chip package, not a from-scratch Xtensa scheduler.

---

## Serial console (UART Micro-USB, 115200)

Plug the **UART** port (next to silk `5V` / `G`).

| Command | Action |
|---|---|
| `help` | list commands |
| `status` / `health` | OS, version, health, heap, tasks, uptime |
| `tasks` | Nexos-RT task count and handles |
| `kernel_info` | full NEXOS-RT banner |
| `boot` | queue splash for the GUI task |
| `test` | queue cyan/green color bars |
| `version` | firmware + OS + pin summary |
| `reboot` | `ESP.restart()` |

`switch_kernel` is **not** supported.

IDF product (when that image is built) also has `wifi …`, `ble …`, `time …`, `display …`, `self-test`, `kernel status/tasks/stats`, `ota …`, `factory_reset`.

---

## Build and flash

```powershell
pio run -e esp32s3_arduino
pio run -e esp32s3_arduino -t upload --upload-port COM5
pio device monitor -p COM5 -b 115200
```

Expected:

```
SMART DEVICE — Nexos-RT V1.2
[KERNEL] Nexos-RT ready.
[BOOT] Nexos-RT branding splash
[PASS] Nexos-RT task GUI  prio=7  core=1
[KERNEL] Nexos-RT running — 3 tasks
SYSTEM READY. Type help.
```

IDF (product image, not the current flash):

```powershell
idf.py set-target esp32s3
powershell -ExecutionPolicy Bypass -File tools/build.ps1
powershell -ExecutionPolicy Bypass -File tools/flash.ps1 -Monitor
```

Desktop simulator / Python tests (simulator only, not the MCU image):

```powershell
python test/run_tests.py
python simulator/run_simulator.py --cli
```

---

## Source layout

```
src/main.cpp                 Arduino dashboard, splash, CLI
src/kernel_manager.cpp       Nexos-RT bring-up, spawn, DisplayGuard
main/arduino_main.cpp        PIO entry (includes src/)
main/app_main.cpp            IDF product
components/microkernel/      Nexos-RT mk_* + ESP32-S3 port
components/board/            pins, BoardConfig
components/display/          IDF GC9A01
components/gui/              LVGL dashboard
components/command/          IDF CommandService
docs/                        architecture and hardening plan
hardware screenshot/         photos + Espressif PDFs
```

---

## Pitfalls

1. VCC on USB **5V** — the TFT VER1.0 module is specified for 3.3 V and can be damaged.
2. RST or CS left floating — connect RST to 14 and CS to 9.
3. SCL/SDA confused with I2C — they are SPI SCLK/MOSI on 12/11.
4. J3 silk `21` is GPIO21, not a supply pin.
5. Duponts one breadboard row off.
6. Serial open in download mode (`boot:0x0 DOWNLOAD`) — reset with IO0 high.

I2C 16/17 is for future sensors/touch only. IMU sip-tracking, UV-C, and deep-sleep hydration UI are **not** in this firmware.

---

---

## 📚 Documentation & Technical References

All architectural, operational, hardware, and engineering documents are indexed below:

### 1. Core System & Hardening Specifications
| Document | Description |
|---|---|
| [COMPLETE_SYSTEM_REFERENCE.md](docs/COMPLETE_SYSTEM_REFERENCE.md) | Canonical system specification, hardware mapping, dual-core partitioning, and wiring rules. |
| [Nexos-RT-Production-Hardening-Plan.md](docs/Nexos-RT-Production-Hardening-Plan.md) | Consolidated multi-agent review & C1–C18 production hardening resolution audit. |
| [CHANGELOG.md](CHANGELOG.md) | Version release history, milestone logs, and changelog tracking. |

### 2. Architecture & Microkernel
| Document | Description |
|---|---|
| [architecture.md](docs/architecture.md) | High-level system layering, boot sequence, and multi-core domain task distribution. |
| [architecture_microkernel.md](docs/architecture_microkernel.md) | Nexos-RT (`mk_*`) microkernel primitives, task slot state machines, spinlocks, and PI mutexes. |
| [NATIVE_KERNEL.md](docs/NATIVE_KERNEL.md) | Pure base-level native kernel mode (`MK_NATIVE_KERNEL=1`), port isolation, and radio quarantine. |


### 3. Hardware & Pinout References
| Document | Description |
|---|---|
| [hardware.md](docs/hardware.md) | ESP32-S3-DevKitC-1 v1.1 pinout, GC9A01 4-wire SPI details, and proven 5-wire connection. |
| [device_mapping_and_state.md](docs/device_mapping_and_state.md) | Hardware identity, memory sizing (342KB SRAM, 8MB PSRAM), and pin mapping logs. |

### 4. Commands, Services & Operations
| Document | Description |
|---|---|
| [commands.md](docs/commands.md) | Interactive command console engine, supported commands, and result object contracts. |
| [ota.md](docs/ota.md) | Dual-slot HTTPS OTA firmware update workflow, signature verification, and rollback. |
| [release-process.md](docs/release-process.md) | Semantic versioning, build metadata, release tagging, and binary generation. |

### 5. Build & Setup Guides
| Document | Description |
|---|---|
| [BUILD_INSTRUCTIONS.md](BUILD_INSTRUCTIONS.md) | Comprehensive step-by-step build, flash, and debug guide for PlatformIO & ESP-IDF. |
| [SETUP_GUIDE.md](SETUP_GUIDE.md) | Developer workstation environment setup, USB CP210x driver installation, and tooling. |
| [FLASH_INSTRUCTIONS.md](release/FLASH_INSTRUCTIONS.md) | Standalone flashing instructions using pre-compiled release binaries and esptool. |

### 6. Repository Metadata & Hardware Manuals
| Resource | Link / Path |
|---|---|
| **License** | [LICENSE](LICENSE) |
| **Current Version** | [VERSION](VERSION) (`1.1.0`) |
| **ESP32-S3 DevKit Guide** | [Espressif DevKitC-1 v1.1 User Guide](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-devkitc-1/user_guide_v1.1.html) |
| **Complete Reference PDF** | [`docs/hardware and toolkit reference doc/Smart-Device-Firmware-Complete-Reference.pdf`](docs/hardware%20and%20toolkit%20reference%20doc/Smart-Device-Firmware-Complete-Reference.pdf) |
| **ESP32-S3 Datasheet** | [`hardware screenshot/esp32-s3_datasheet_en.pdf`](hardware%20screenshot/esp32-s3_datasheet_en.pdf) |
| **DevKit Hardware Manual** | [`hardware screenshot/esp-dev-kits-en-master-esp32s3.pdf`](hardware%20screenshot/esp-dev-kits-en-master-esp32s3.pdf) |
| **Pinout Diagram** | [`hardware screenshot/ESP32-S3_DevKitC-1_pinlayout_v1.1.jpg`](hardware%20screenshot/ESP32-S3_DevKitC-1_pinlayout_v1.1.jpg) |

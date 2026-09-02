# Nexos-RT Smart Device — Client Demo Brief

**Product:** Smart round-display device firmware  
**OS:** Nexos-RT V1.2.0  
**Firmware:** 1.2.0  
**Hardware:** ESP32-S3-DevKitC-1 v1.1 + 1.28" GC9A01 240×240 TFT  
**Audience:** Client demo / architecture walkthrough  
**Date:** September 2026  

This document is the single briefing pack for a live hardware demo: what the product is, what is running on the board today, how it is built, and a step-by-step script you can present.

---

## 1. One-minute pitch

This is a **smart wearable-class display node**: a round 240×240 glass, a dual-core ESP32-S3, and a custom application OS called **Nexos-RT**.

On power-up the device:

1. Boots its own OS (not a raw Arduino sketch with no structure).
2. Plays a branded **NEXOS-RT** splash (kernel → scheduler → drivers → display → ready).
3. Lands on a live circular dashboard.
4. Advertises itself over **Bluetooth** as `SmartDisplay`.
5. Broadcasts a **2.4 GHz Wi-Fi hotspot** named `SmartDisplay-XXXX` so a phone can find it without a home router.

The dashboard shows **Wi-Fi ON/OFF** and **BLE ON/OFF** with the same colour language, plus OS health, uptime, heap, and the hotspot name.

**What this proves to a client**

| Claim | Evidence on the table |
|---|---|
| Real hardware, not a mock | Round GC9A01 glass + ESP32-S3 DevKit, live |
| Product OS, not a throwaway sketch | Nexos-RT tasks, watchdog, mutex, serial `kernel_info` |
| Phone can discover the device | Wi-Fi list + BLE scanner |
| Dual connectivity | SoftAP hotspot + BLE Nordic UART |
| Production path exists | Same OS on an LVGL 9.5 / ESP-IDF product image |

---

## 2. What you are demoing today

There are **two firmware tracks** in the repository. **Only the Arduino bring-up image is flashed on the demo board.**

| | **Live demo image (on the chip)** | **Product track (same repo)** |
|---|---|---|
| Build env | PlatformIO `esp32s3_arduino` | PlatformIO `esp32s3_idf` / ESP-IDF |
| Entry | `src/main.cpp` | `main/app_main.cpp` |
| Display | Adafruit GFX, hardware SPI @ **8 MHz** | `esp_lcd` + GC9A01 + **LVGL 9.5** |
| UI | Circular dashboard + FreeSans Bold clock | Glassmorphism LVGL dashboard |
| OS | **Nexos-RT V1.2 only** | **Nexos-RT V1.2 only** |
| Console | UART 115200 (`help`, `wifi_*`, `ble_*`) | Unified `CommandService` |
| Connectivity | BLE + Wi-Fi hotspot (and optional STA) | Wi-Fi, BLE, SNTP, OTA, NVS |
| Status | **This is what the client sees** | Next visual upgrade on the same OS |

Nexos-RT is the **only** application OS. There is no dual-kernel switch. Arduino-ESP32 / ESP-IDF only supply chip drivers (Wi-Fi, BLE, SPI, flash) through a quarantined port. They are not a second product kernel.

---

## 3. Live demo script (10–12 minutes)

Use this order. Do not open a serial monitor until step 6 if you want a clean first impression — the CP210x auto-reset line can bounce the board.

### Step 1 — Power (30 s)

- Plug **UART USB-C** (the port next to silk `5V` / `G`, Silicon Labs CP210x).
- Press **EN / RST** once.

**Say:** “Power on. The panel is a 1.28 inch round GC9A01. Backlight is tied to 5 V. You should see a bright white–red–green–blue lamp test, then the OS splash.”

**Expect**

1. White → red → green → blue (panel + SPI proof).
2. NEXOS-RT splash: rings, **N** mark, **BASE OS V1.2**, bar: KERNEL CORE → SCHEDULER → DRIVERS → DISPLAY → READY.
3. Dashboard.

If the glass stays dull blue, the panel has power but SPI is not writing. Check VCC=5V, GND, SCL=12, SDA=11, DC=10.

### Step 2 — Dashboard walkthrough (2 min)

Point at the glass, top to bottom:

| Region | What it is | Talking point |
|---|---|---|
| Top pill | **OS OK** (green) / **OS ERR** (red) | Kernel health, not a fake LED |
| Large clock | FreeSans Bold 18 pt | Uptime until NTP; local time after Wi-Fi STA + SNTP |
| Cyan caption | `SYSTEM UPTIME` or `LOCAL TIME` | Time domain of the OS |
| Middle banner | Hotspot name `SmartDisplay-XXXX` | This is the name phones will scan |
| Status card | Tasks, heap, `[OK]`, `UP mm:ss` | Live OS telemetry |
| Bottom left | **WIFI AP / WIFI ON / WIFI OFF** | Same visual language as BLE |
| Bottom right | **BLE ADV / BLE ON / BLE OFF** | Advertising vs connected |

**Colour language (same for Wi-Fi and BLE)**

| Colour | Meaning |
|---|---|
| **Green** | Connected (phone on hotspot, or BLE client linked) |
| **Cyan** | Service on, waiting (hotspot up, no client yet) |
| **Orange** | In progress (joining a router, or BLE advertising) |
| **Red** | Off |

**Timing after splash**

- Dashboard is immediate.
- **BLE ADV** (orange) appears after about **2.5 s**.
- **WIFI AP** (cyan) appears after about **6 s**.
- Uptime in the card must **keep counting**. If it jumps to `00:00`, the chip reset.

### Step 3 — Wi-Fi discovery on a phone (3 min)

This is the headline connectivity demo.

1. Wait until the glass shows **WIFI AP** (cyan) and the banner `SmartDisplay-XXXX`.
2. On **Android or iPhone**: Settings → **Wi-Fi** (not Bluetooth).
3. Look on the **2.4 GHz** list. ESP32-S3 has no 5 GHz radio.

| Field | Value on this board |
|---|---|
| SSID | `SmartDisplay-1D78` (last two MAC bytes; MAC is `1c:db:d4:9c:1d:78`) |
| Password | `nexos1234` |
| Security | WPA2 |
| Device IP after join | `192.168.4.1` |
| Band | 2.4 GHz, channel 6 |

4. Join the network.
5. Glass should change **WIFI AP** (cyan) → **WIFI ON** (green). Banner shows `n in` (client count).

**Say:** “The device is an access point. You do not need a home router for first-time pairing. The name on the glass is the name in the phone’s Wi-Fi list.”

**If the phone cannot see it**

- iPhone often stays on 5 GHz — pull down Wi-Fi and wait, or disable 5 GHz-only.
- Look under Wi-Fi, not Bluetooth. The BLE name is `SmartDisplay`; the Wi-Fi name is `SmartDisplay-1D78`.
- Stand within a few metres; TX power is moderated so USB-powered boards do not brown out.

### Step 4 — Bluetooth (2 min)

1. Glass should show **BLE ADV** (orange).
2. **Android:** Settings → Bluetooth, look for `SmartDisplay`.  
   **iPhone:** Settings → Bluetooth often **hides generic GATT devices**. Use **nRF Connect** or **LightBlue**.
3. Connect.
4. Glass: **BLE ADV** → **BLE ON** (green).
5. Optional: write a short ASCII string to the Nordic UART RX characteristic. Device echoes `ECHO: …` and prints `[BLE RX]` on serial.

| BLE item | Value |
|---|---|
| GAP name | `SmartDisplay` |
| Profile | Nordic UART Service (NUS) |
| Service UUID | `6E400001-B5A3-F393-E0A9-E50E24DCCA9E` |
| RX (phone → device) | `6E400002-…` Write + Write Without Response |
| TX (device → phone) | `6E400003-…` Notify + Read |

**Say:** “Wi-Fi is how a phone finds the device as a network. BLE is the low-energy control pipe. Both states are on the glass at the same time.”

### Step 5 — OS is real (1 min)

On the glass, point at **OS OK**, **TASKS**, **HEAP**, **UP**.

**Say:** “Three application tasks on core 1: GUI, SYSTEM, CLI. Core 0 is reserved for the radio stacks. The application never calls FreeRTOS APIs directly — it uses `mk_*` from Nexos-RT.”

### Step 6 — Serial console (2 min, optional but strong)

115200 baud on **COM5** (this PC). Type `help`.

Suggested live commands:

```
help
status
kernel_info
wifi_ap
ble_status
boot
test
version
```

| Command | What the client sees |
|---|---|
| `status` | OS name, health, heap, task count, uptime, Wi-Fi, BLE |
| `kernel_info` | Full Nexos-RT banner |
| `wifi_ap` / `wifi_status` | Hotspot SSID, password, AP IP, client count |
| `ble_status` | Advertising / connected / name |
| `boot` | Splash replays on the glass (GUI owns the TFT) |
| `test` | Cyan / green colour bars |
| `wifi_scan` | Nearby 2.4 GHz networks |
| `wifi_connect <ssid> <pass>` | Join a home router (STA), then SNTP time |

---

## 4. Hardware (what is on the table)

### 4.1 Compute

| Item | Spec |
|---|---|
| Board | ESP32-S3-DevKitC-1 **v1.1** |
| Module | ESP32-S3-WROOM-1 **N8R8** |
| CPU | Dual Xtensa LX7 @ **240 MHz** |
| Flash | 8 MB |
| PSRAM | 8 MB octal |
| SRAM | ~342 KB internal |
| USB UART | CP210x, **COM5**, VID:PID `10C4:EA60` |
| Native USB | Second USB-C (not used for this demo console) |

### 4.2 Display

| Item | Spec |
|---|---|
| Panel | 1.28" round TFT VER1.0 |
| Controller | **GC9A01**, 240×240 |
| Bus | 4-wire SPI (silk SCL/SDA are SPI CLK/MOSI, **not I2C**) |
| Backlight | Tied to **VCC** (no BLK pin) |
| VCC | **5 V** (onboard regulator on this breakout) |
| Logic | 3.3 V SPI from the ESP32 |

### 4.3 Demo wiring (proven 5-wire)

Display header order: `VCC  GND  SCL  SDA  DC  CS  RST`.

| Display | ESP32-S3 silk | Wired on demo unit? |
|---|---|---|
| **VCC** | **5V** | Yes |
| **GND** | **G** | Yes |
| **SCL** (SPI clock) | **12** | Yes |
| **SDA** (SPI MOSI) | **11** | Yes |
| **DC** | **10** | Yes |
| CS | 9 | Open (module strap / software path) |
| RST | 14 | Open (software reset `0x01`) |

CS and RST may stay unplugged on this panel revision. Firmware treats them as unused (`-1`) and software-resets the controller so a warm ESP32 reboot still re-inits the glass.

**Do not use:** GPIO 35/36/37 (octal PSRAM), 19/20 (native USB), 43/44 (UART0 console), J3 silk `21` (that is GPIO21, not 5 V).

---

## 5. Architecture (presentation diagram)

```
┌─────────────────────────────────────────────────────────────┐
│  Application                                                │
│  Dashboard · splash · CLI · Wi-Fi hotspot · BLE NUS         │
│  src/main.cpp  (live image)                                 │
└──────────────────────────┬──────────────────────────────────┘
                           │  include mk.h only
                           ▼
┌─────────────────────────────────────────────────────────────┐
│  Nexos-RT V1.2   mk_*                                       │
│  tasks · mutex (priority inheritance) · queue · event       │
│  timer · pool · watchdog · launch gate                      │
│  components/microkernel/                                    │
└──────────────────────────┬──────────────────────────────────┘
                           │  mk_chip_port.h (internal)
                           ▼
┌─────────────────────────────────────────────────────────────┐
│  ESP32-S3 chip support                                      │
│  Arduino-ESP32 / ESP-IDF drivers                            │
│  Wi-Fi · BLE · SPI · UART · flash                           │
└──────────────────────────┬──────────────────────────────────┘
                           ▼
              ESP32-S3-WROOM-1 N8R8  +  GC9A01
```

### 5.1 Why Nexos-RT exists

Client-facing answer: **the product owns the OS contract.** Application code is written against `mk_task_create`, `mk_mutex_lock`, `mk_watchdog_feed`, not against a vendor RTOS API. That means:

- Task layout, priorities, and watchdog policy are product-defined.
- Display access is serialized (`DisplayGuard`) so CLI and GUI cannot tear the frame.
- The same `mk_*` API is used on the Arduino bring-up image **and** the IDF/LVGL product image.
- Vendor Wi-Fi/BLE stay on **core 0**; product work stays on **core 1**.

Internally this bring-up still **links** the ESP32 vendor runtime for the chip (Wi-Fi and Bluedroid require it). We do not claim the binary is FreeRTOS-free. We claim the **application does not program FreeRTOS**.

### 5.2 Boot sequence (live image)

```
POWER ON
  → ROM / second-stage bootloader
  → Partition table (app at 0x10000)
  → KernelManager::init()     NVS, mk_init, diagnostics, display mutex
  → SPI.begin(SCLK=12, MOSI=11) + GC9A01 begin @ 8 MHz
  → Software panel reset, max brightness, VREG boost
  → Colour lamp test (white / red / green / blue)
  → NEXOS-RT splash (5 stages)
  → spawn GUI, SYSTEM, CLI on core 1 (they wait on the launch gate)
  → mk_start() opens the gate
  → Dashboard
  → +2.5 s  BLE advertising  (SmartDisplay)
  → +6 s    Wi-Fi SoftAP     (SmartDisplay-XXXX)
```

Radios start **after** the dashboard is up so a Wi-Fi/BLE stack delay cannot hide a dead UI.

### 5.3 Tasks on the live image

| Task | Nexos-RT prio | Port prio | Core | Stack | Role |
|---|---|---|---|---|---|
| **GUI** | 7 | 16 | 1 | 8 KB | Splash, dashboard, 1 Hz refresh |
| **SYSTEM** | 3 | 12 | 1 | 12 KB | Health, uptime, delayed BLE + SoftAP |
| **CLI** | 6 | 10 | 1 | 6 KB | UART command engine |
| *(vendor)* | — | high | **0** | — | Wi-Fi, BLE host, LwIP |

### 5.4 Kernel mechanics worth naming in Q&A

- **Launch gate:** tasks are created first, then `mk_start()` releases them (no run-before-register race).
- **Task slots:** FREE → RESERVED → LIVE → DELETING, SMP spinlock.
- **DisplayGuard:** RAII lock with timeout; GUI owns the TFT.
- **Watchdog API:** `mk_watchdog_feed_self()`; health text on the glass (`OS OK`).
- **Heap accounting:** internal SRAM only, so PSRAM cannot hide a real RAM problem.
- **Tick:** 1 ms (`MK_CONFIG_TICK_HZ 1000`).
- **Mutex priority inheritance** compiled in.

### 5.5 Dual-core split

```
Core 1 — product                    Core 0 — chip / radio
GUI · SYSTEM · CLI                  Wi-Fi MAC / LwIP
mk scheduler + app mutex            BLE host (Bluedroid)
SPI to GC9A01                       UART driver, interrupts
```

---

## 6. Connectivity architecture

```
                    ┌──────────────┐
   Phone Wi-Fi ────►│ SoftAP       │  SSID SmartDisplay-XXXX
                    │ 192.168.4.1  │  WPA2  nexos1234
                    └──────┬───────┘
                           │  2.4 GHz
                    ┌──────┴───────┐
                    │ ESP32-S3 RF  │
                    └──────┬───────┘
                           │
   Phone BLE  ────►┌───────┴───────┐
                   │ BLE peripheral│  name SmartDisplay
                   │ Nordic UART   │  echo + future commands
                   └───────────────┘
```

**Design rules used on this board**

- SoftAP is **AP-only** until the operator runs `wifi_connect` (then AP+STA). Starting AP+STA and BLE in the same instant reset this hardware.
- BLE comes up first (~2.5 s), hotspot later (~6 s).
- Wi-Fi modem sleep stays on so BLE can share the 2.4 GHz radio.
- TX power is moderated (USB-hub brown-out protection).
- Hotspot name is derived from the STA MAC so two boards never collide.

**Optional next command (STA / internet)**

```
wifi_connect <home-ssid> <password>
```

When STA is up: banner shows `IP x.x.x.x`, SNTP syncs **IST (+5:30)**, clock caption becomes `LOCAL TIME`.

---

## 7. User interface (what “good glass” means)

The live image is Adafruit GFX (bring-up). The IDF image is LVGL 9.5 (product visual). Both are the same OS.

**Live dashboard (Arduino)**

- Round bezel, cyan rings.
- Clock in **FreeSans Bold 18 pt** (not the old 5×7 bitmap font).
- Opaque text (no smear / ghosting).
- Brighter panel registers (display brightness `0xFF`, slightly higher VREG).
- SPI at **8 MHz** (was 2 MHz; 2 MHz was only a prototype margin setting).
- Matching **WIFI** and **BLE** status pills.

**Product dashboard (IDF / LVGL — show a screenshot if the Arduino glass is the only live unit)**

- Glassmorphism cards, Montserrat type, 12-hour / 24-hour clock, command PASS/FAIL chip, heap bar.
- Same Nexos-RT task model.

If a client asks “why isn’t this LVGL right now?”: the Arduino image is the **hardware bring-up** that is proven on this wiring. LVGL is the **product UI** on the same pins and the same OS, built from `esp32s3_idf`.

---

## 8. Serial command sheet (handout)

Port: UART USB-C · **115200 8N1** · this unit **COM5**

```
help
status / health
version
kernel_info
tasks
wifi_status / wifi / wifi_ap
wifi_scan
wifi_connect <ssid> <pass>
wifi_disconnect
ble_status
ble_start
ble_stop
time_status
time_sync
boot
test
reboot
```

`switch_kernel` is **not** a product command.

---

## 9. Memory and flash map (live Arduino image)

| Region | Offset | Role |
|---|---|---|
| Bootloader | `0x00000` | ESP32-S3 second stage |
| Partition table | `0x08000` | Custom 8 MB OTA table |
| otadata | `0x0E000` | OTA slot select |
| **ota_0 (this app)** | **`0x10000`** | Live firmware (magic `0xE9`) |
| ota_1 | `0x310000` | Second OTA slot |
| storage | `0x610000` | SPIFFS-style data |

The app **must** sit at `0x10000` on the Arduino env. A table that claimed `0x20000` while the uploader wrote `0x10000` produced a reset loop and a dim blue glass (backlight on, no pixels). That is fixed on this unit.

---

## 10. Product roadmap (honest)

| Now (demo unit) | Next on the same OS |
|---|---|
| Arduino bring-up dashboard | LVGL 9.5 product dashboard |
| BLE echo (NUS) | BLE RX routed into `CommandService` |
| SoftAP for discovery | Captive portal / provisioning |
| Optional `wifi_connect` + SNTP | Persistent NVS credentials, auto-join |
| UART console | Same commands on UART, USB CDC, BLE, Wi-Fi |
| Manual flash | HTTPS OTA with rollback (IDF image already has the service) |
| 5-wire display | CS + RST wired for factory (GPIO 9 / 14) |
| I2C 16/17 unused | Sensors (IMU, temperature, fuel gauge) |

---

## 11. Demo-day checklist

**Before the client sits down**

- [ ] UART USB-C in the **UART** port (not the native USB port)
- [ ] VCC on **5V**, GND on **G**, SCL **12**, SDA **11**, DC **10**
- [ ] Press **EN/RST**, confirm splash + dashboard
- [ ] Wait 6 s, confirm **WIFI AP** + **BLE ADV**
- [ ] Phone 2.4 GHz Wi-Fi list shows `SmartDisplay-1D78`
- [ ] Serial monitor **closed** until you need commands (avoids CP210x reset chatter)
- [ ] Password card ready: `nexos1234`

**Opening line**

> “This is Nexos-RT V1.2 on an ESP32-S3 with a round GC9A01. The OS owns the tasks. The glass is the product face. Wi-Fi and Bluetooth are how a phone finds it.”

**Closing line**

> “What you saw is the bring-up image: real OS, real radios, real glass. The same Nexos-RT API carries the LVGL product UI, OTA, and unified commands.”

---

## 12. Q&A (short answers)

**Is this FreeRTOS?**  
The chip vendor runtime is present for Wi-Fi and BLE. The **application** is written only to Nexos-RT (`mk_*`). There is one product OS.

**Why a round display?**  
Wearable / smart-device form factor. 240×240 GC9A01 is the production panel family.

**Why can’t iPhone Settings show BLE?**  
Apple Settings lists audio/HID, not generic GATT. Use nRF Connect / LightBlue. Wi-Fi Settings **does** list `SmartDisplay-1D78`.

**Why 2.4 GHz only?**  
ESP32-S3 radio is 2.4 GHz. That is also the right band for a small hotspot.

**Can we put this in a custom PCB?**  
Yes. Pin map is FSPI-native (11/12/10). CS=9 and RST=14 are the factory recommendation. 5 V only because **this** TFT breakout has an onboard regulator.

**Is OTA in the demo binary?**  
Not in the Arduino bring-up image. OTA with rollback lives on the IDF product track, same OS.

**What if the glass is dim or snowy?**  
Dim blue = no SPI (wrong pins or board not booting). Snow = SPI too fast for long jumpers (drop toward 4 MHz) or CS floating on a module without a strap. This demo unit runs 8 MHz on the proven 5-wire harness.

---

## 13. Identity card (print / slide footer)

```
Nexos-RT Smart Device     Firmware 1.2.0     OS Nexos-RT V1.2.0
ESP32-S3-WROOM-1 N8R8     GC9A01 240×240     UART 115200 COM5
Wi-Fi  SmartDisplay-1D78  pass nexos1234     192.168.4.1  2.4 GHz
BLE    SmartDisplay       Nordic UART        6E400001-…
```

---

*Internal repo paths for the presenting engineer: live image `src/main.cpp`, OS `components/microkernel/`, IDF product `main/app_main.cpp`, pins `docs/hardware.md`.*

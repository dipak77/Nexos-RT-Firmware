# ⚡ Hardware Flash & Quick Setup Guide (GC9A01 7-Pin Module)

Complete guide to flashing and running the **Smart Device Firmware v1.2.0** onto your **ESP32-S3** with the **GC9A01 7-Pin 240x240 Round Display**.

---

## 🔌 Exact 7-Pin Hardware Wiring Map

Connect the round GC9A01 module to the ESP32-S3 as shown below. CS and RST are
recommended; they may remain unplugged only when this VER1.0 PCB's onboard straps are fitted.

```
+-------------------------------------------------------------+
|    GC9A01 7-Pin Module         --->          ESP32-S3       |
+-------------------------------------------------------------+
|  1. RST (Reset)                 --->   GPIO 14              |
|  2. CS  (Chip Select)           --->   GPIO 9               |
|  3. DC  (Data / Command)        --->   GPIO 10              |
|  4. SDA (SPI MOSI Data)         --->   GPIO 11              |
|  5. SCL (SPI Clock)             --->   GPIO 12              |
|  6. GND (Ground)                --->   GND                  |
|  7. VCC (Breakout power input)  --->   5V                   |
+-------------------------------------------------------------+
```

*(This photographed TFT VER1.0 breakout has an onboard regulator and accepts 5 V at VCC. Its SPI signals are still driven at the ESP32's 3.3 V GPIO level. There is no separate BLK pin.)*

---

## 📦 Flash Binary Files & Memory Map

Target Chip: **ESP32-S3-WROOM-1 N8R8** (8 MB flash, 8 MB PSRAM)

| Binary File | Flash Offset | Description |
|---|---|---|
| **`firmware_all_in_one.bin`** | **`0x0000`** | **Single merged binary (Contains Bootloader + Partitions + App)** |
| `bootloader.bin` | `0x0000` | ESP32-S3 2nd Stage Bootloader |
| `partition-table.bin` | `0x8000` | Partition Table Map |
| `smart_device_firmware.bin` | `0x20000` | OTA slot 0 application binary (GC9A01 driver) |

---

## 🚀 1-Click Hardware Flash (Windows)

### Method A: Double-Click Batch File
1. Plug your ESP32-S3 into your PC via USB-C cable.
2. In File Explorer, go to the `release` folder and double-click:
   👉 **`flash_device.bat`**
3. It will auto-detect your COM port (e.g. `COM5`), flash the device at high speed, and launch the serial console.

### Method B: PowerShell One-Liner
```powershell
powershell -ExecutionPolicy Bypass -File release/flash_device.ps1 -Port COM5
```

---

## 🌐 Option 2: Flash Directly in Browser (ESP Web Flasher)

1. Open **[https://espressif.github.io/esptool-js/](https://espressif.github.io/esptool-js/)** in Chrome or Edge.
2. Click **Connect** and select your ESP32-S3 serial port (`COM5`).
3. Add **`release/firmware_all_in_one.bin`** at offset **`0x0`** (or `0x0000`).
4. Click **Program / Flash**.
5. Once complete, press the **RST / EN** button on the ESP32-S3.

---

## 🖥️ Live Hardware Console Output

```text
=========================================================
 SMART DEVICE — Nexos-RT V1.2
 GC9A01 HW SPI 2MHz  SCL=12 SDA=11 DC=10 CS=9 RST=14
 TFT VER1.0 breakout VCC=5V; SPI GPIO levels=3V3
=========================================================
[DISPLAY] Adafruit GC9A01A hardware SPI begin at 2000000 Hz
[DISPLAY] startup test RED -> GREEN -> BLUE -> BLACK
[BOOT] Nexos-RT branding splash
SYSTEM READY. Type help.
```

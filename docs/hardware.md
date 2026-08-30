# Hardware (from photos)

## MCU
ESP32-S3-DevKitC-1 **v1.1**
Module: ESP32-S3-WROOM-1 **N8R8** (silk `MCN8R8`)
RGB LED: **GPIO38** (`RGB@IO38` on PCB)
UART: left Micro-USB (CP210x). Native USB is the right connector.
esptool: ESP32-S3 QFN56 rev 0.2, 8 MB octal PSRAM 3.3 V, 40 MHz crystal.

GPIO35/36/37 are used by octal PSRAM — do not wire.

## Display is 4-wire SPI, not I2C
1.28" TFT VER1.0, 240×240, **IC: GC9A01**

The round PCB is printed **SPI** next to VCC. Silk names `SCL` / `SDA` are borrowed from I2C but they are **SPI clock** and **SPI MOSI**. Proof:

- GC9A01 on this module talks **4-wire SPI** (Waveshare and the IC datasheet).
- Extra pins **DC** and **CS** exist only on SPI panels. I2C would be VCC, GND, SDA, SCL (and maybe RST) — no DC, no CS.
- Some boards fit an optional CS resistor, but production wiring still drives CS and RST explicitly.

7-pin header silk **from VCC toward RST**:

`VCC  GND  SCL  SDA  DC  CS  RST`

No BLK pin. Backlight LED is powered from **VCC**. The TFT VER1.0 / GC9A01 module is specified for **3.3 V operation**; connect VCC to a DevKit **3V3** pin. Do not use USB 5 V.

I2C on this project (GPIO 16/17, or the old README 5/6) is **expansion for sensors/touch only**. Do not wire the LCD SDA/SCL there.

## Wiring (7-Wire Setup)

| Display silk | Meaning | J1 Pin | J1 silk | ESP32-S3 GPIO | Status / Wiring Rule |
|---|---|---|---|---|---|
| **VCC** | Backlight + panel power | 1 or 2 | **3V3** | **3.3 V** | Direct wire; never USB 5 V |
| **GND** | Ground | 22 | **G** | **GND** | Direct wire |
| **SCL** | SPI Clock | 18 | **12** | **GPIO 12** | Direct wire |
| **SDA** | SPI MOSI | 17 | **11** | **GPIO 11** | Direct wire |
| **DC** | Data / Command | 16 | **10** | **GPIO 10** | Direct wire |
| **CS** | Chip Select | 15 | **9** | **GPIO 9** | Direct wire; active low |
| **RST** | Reset | 20 | **14** | **GPIO 14** | Direct wire; active low |

> [!NOTE]
> GPIO 13 is FSPI MISO and is not connected to this LCD (write-only display pipeline).
> I2C Expansion (GPIO 16/17) is reserved for external sensors (IMU, Temperature, Fuel Gauge).

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
- R8 on the back is a **CS pull-down**. The Chinese note says if R8 is fitted, CS and RST may be left open. We still drive both.

7-pin header silk **from VCC toward RST**:

`VCC  GND  SCL  SDA  DC  CS  RST`

No BLK pin. Backlight LED is powered from **VCC**. Use **5V on J1-21**. 3.3 V on J1-1 often leaves the LED off while the MCU still runs. Logic is 3.3 V; VCC may be 3.3 V or 5 V on this board — 5 V is required for a visible backlight on the unit we have.

I2C on this project (GPIO 16/17, or the old README 5/6) is **expansion for sensors/touch only**. Do not wire the LCD SDA/SCL there.

## Wiring (native FSPI / SPI2)

| Display silk | Meaning | J1 silk | GPIO |
|---|---|---|---|
| VCC | backlight + panel power | 21 | **5V** |
| GND | ground | 22 | **G** |
| SCL | SPI CLK | 12 | **12** |
| SDA | SPI MOSI | 11 | **11** |
| DC | data/command | 10 | **10** |
| CS | chip select (active low, must toggle) | 9 | **9** |
| RST | hardware reset | 14 | **14** |

GPIO 13 is FSPI MISO and is not connected to this LCD (write-only).

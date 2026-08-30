#pragma once
// ESP32-S3-DevKitC-1 v1.1 + 1.28" TFT VER1.0 GC9A01
// Display is 4-wire SPI. Silk SCL/SDA are SPI CLK/MOSI, not I2C.
// Silk from VCC toward RST: VCC, GND, SCL, SDA, DC, CS, RST
// No BLK pin — backlight LED is tied to VCC. Feed VCC from 5V (J1-21).
// Pins below are native FSPI (SPI2) IOMUX: 11 MOSI, 12 SCLK, 13 MISO, 10 SS.

#define LCD_PIN_MOSI        11  // SDA  J1-11  SPI MOSI
#define LCD_PIN_SCLK        12  // SCL  J1-12  SPI CLK
#define LCD_PIN_CS          9   // CS   GPIO9. R8 pulls CS low if unconnected; still drive CS so RAMWR gets a CS edge.
#define LCD_PIN_DC          10  // DC   J1-10
#define LCD_PIN_RST         -1  // This module POR's itself. GPIO14 sits next to 5V/GND; driving RST blanks the panel.
#define LCD_PIN_BL         -1

#define LCD_HRES           240
#define LCD_VRES           240
// Long breadboard jumpers on the prototype showed intermittent uninitialized
// frame-RAM "snow".  Start conservatively; 2 MHz is sufficient for the mostly
// static dashboard and provides substantially more signal margin than 8 MHz.
#define LCD_PIXEL_CLOCK_HZ (2 * 1000 * 1000)
#define LCD_CMD_BITS       8
#define LCD_PARAM_BITS     8

// I2C expansion only (sensors / touch). NOT the LCD.
#define I2C_EXP_SDA        16
#define I2C_EXP_SCL        17
#define I2C_EXP_FREQ_HZ  400000
#define I2C_EXP_PORT            0  // I2C_NUM_0; keep board config driver-independent

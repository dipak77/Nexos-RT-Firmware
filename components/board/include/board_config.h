#pragma once
#include "board_pins.h"

namespace smart_device {
namespace board {

struct BoardConfig {
    int lcd_mosi;
    int lcd_clk;
    int lcd_cs;
    int lcd_dc;
    int lcd_reset;
    int lcd_bl;
    int lcd_hres;
    int lcd_vres;
    int lcd_pixel_clock_hz;

    int i2c_sda;
    int i2c_scl;
    int i2c_port;
    int i2c_freq_hz;

    const char* name;
};

constexpr BoardConfig ESP32_S3_DEVKITC_V11 = {
    .lcd_mosi = LCD_PIN_MOSI,
    .lcd_clk = LCD_PIN_SCLK,
    .lcd_cs = LCD_PIN_CS,
    .lcd_dc = LCD_PIN_DC,
    .lcd_reset = LCD_PIN_RST,
    .lcd_bl = LCD_PIN_BL,
    .lcd_hres = LCD_HRES,
    .lcd_vres = LCD_VRES,
    .lcd_pixel_clock_hz = LCD_PIXEL_CLOCK_HZ,
    .i2c_sda = I2C_EXP_SDA,
    .i2c_scl = I2C_EXP_SCL,
    .i2c_port = I2C_EXP_PORT,
    .i2c_freq_hz = I2C_EXP_FREQ_HZ,
    .name = "ESP32-S3-DevKitC-1 v1.1 GC9A01"
};

} // namespace board
} // namespace smart_device

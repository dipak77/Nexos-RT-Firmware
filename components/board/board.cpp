#include "board.h"
#include "board_config.h"
#include "esp_log.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_psram.h"
#include <cstring>

static const char* TAG = "BOARD";

namespace smart_device {
namespace board {

Board& Board::instance() {
    static Board inst;
    return inst;
}

Result<void> Board::initialize(const BoardConfig& config) {
    if (initialized_) return Result<void>::Ok();
    config_ = config;
    ESP_LOGI(TAG, "Board: %s", config_.name);
    ESP_LOGI(TAG, "LCD: MOSI=%d CLK=%d CS=%d DC=%d RST=%d %dx%d @%dHz",
             config_.lcd_mosi, config_.lcd_clk, config_.lcd_cs,
             config_.lcd_dc, config_.lcd_reset,
             config_.lcd_hres, config_.lcd_vres, config_.lcd_pixel_clock_hz);

    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    uint32_t flash_size = 0;
    esp_flash_get_size(nullptr, &flash_size);
    size_t psram_size = esp_psram_get_size();

    ESP_LOGI(TAG, "Chip: %s cores=%d rev=%d flash=%luMB PSRAM=%uKB",
             (chip_info.model == CHIP_ESP32S3) ? "ESP32-S3" : "Unknown",
             chip_info.cores, chip_info.revision,
             flash_size / (1024*1024),
             (unsigned)(psram_size/1024));

    ESP_LOGI(TAG, "I2C Expansion: SDA=%d SCL=%d freq=%d port=%d",
             config_.i2c_sda, config_.i2c_scl, config_.i2c_freq_hz, config_.i2c_port);

    initialized_ = true;
    return Result<void>::Ok();
}

void Board::print_info() const {
    ESP_LOGI(TAG, "================ Board Info ================");
    ESP_LOGI(TAG, " Name: %s", config_.name);
    ESP_LOGI(TAG, " LCD: %dx%d", config_.lcd_hres, config_.lcd_vres);
    ESP_LOGI(TAG, "============================================");
}

} // namespace board
} // namespace smart_device

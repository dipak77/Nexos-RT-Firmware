#include "spi_hal.h"
#include "esp_log.h"
#include "driver/spi_master.h"

static const char* TAG = "SPI_HAL";

namespace smart_device {
namespace hal {

SpiHal& SpiHal::instance(){ static SpiHal s; return s; }

Result<void> SpiHal::initialize(const board::BoardConfig& cfg){
    if(initialized_) return Result<void>::Ok();
    spi_bus_config_t bus_cfg{};
    bus_cfg.mosi_io_num = cfg.lcd_mosi;
    bus_cfg.miso_io_num = -1;
    bus_cfg.sclk_io_num = cfg.lcd_clk;
    bus_cfg.quadwp_io_num = -1;
    bus_cfg.quadhd_io_num = -1;
    bus_cfg.max_transfer_sz = cfg.lcd_hres * 80 * 2;
    bus_cfg.flags = SPICOMMON_BUSFLAG_MASTER;

    // CS is owned by esp_lcd panel IO, not the SPI bus driver
    esp_err_t ret = spi_bus_initialize(SPI2_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    if(ret!=ESP_OK){
        ESP_LOGE(TAG, "SPI bus init failed: %s", esp_err_to_name(ret));
        return Result<void>::Err(AppError::SPI_BUS_INIT_FAILED, esp_err_to_name(ret));
    }
    initialized_ = true;
    ESP_LOGI(TAG, "SPI bus initialized MOSI=%d CLK=%d max_xfer=%d", cfg.lcd_mosi, cfg.lcd_clk, (int)bus_cfg.max_transfer_sz);
    return Result<void>::Ok();
}

} // namespace hal
} // namespace smart_device

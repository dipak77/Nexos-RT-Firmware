#include "i2c_hal.h"
#include "esp_log.h"

static const char* TAG = "I2C_HAL";

namespace smart_device {
namespace hal {

I2cHal& I2cHal::instance(){ static I2cHal s; return s; }

Result<void> I2cHal::initialize(const board::BoardConfig& cfg){
    if(initialized_) return Result<void>::Ok();
    i2c_master_bus_config_t conf{};
    conf.i2c_port = cfg.i2c_port;
    conf.sda_io_num = static_cast<gpio_num_t>(cfg.i2c_sda);
    conf.scl_io_num = static_cast<gpio_num_t>(cfg.i2c_scl);
    conf.clk_source = I2C_CLK_SRC_DEFAULT;
    conf.glitch_ignore_cnt = 7;
    conf.flags.enable_internal_pullup = true;

    esp_err_t ret = i2c_new_master_bus(&conf, &bus_);
    if(ret!=ESP_OK) return Result<void>::Err(AppError::I2C_BUS_INIT_FAILED, esp_err_to_name(ret));
    port_ = cfg.i2c_port;
    initialized_ = true;
    ESP_LOGI(TAG, "I2C initialized SDA=%d SCL=%d freq=%d port=%d", cfg.i2c_sda, cfg.i2c_scl, cfg.i2c_freq_hz, cfg.i2c_port);
    return Result<void>::Ok();
}

Result<void> I2cHal::scan(){
    if(!initialized_ || !bus_) {
        return Result<void>::Err(AppError::I2C_BUS_INIT_FAILED, "I2C bus not initialized");
    }
    ESP_LOGI(TAG, "I2C scan on port %d", port_);
    for(int addr=3; addr<0x78; ++addr){
        esp_err_t ret = i2c_master_probe(bus_, static_cast<uint16_t>(addr), 100);
        if(ret==ESP_OK) ESP_LOGI(TAG, "Found device at 0x%02X", addr);
        else if(ret==ESP_ERR_TIMEOUT) {
            ESP_LOGW(TAG, "I2C bus timeout while probing 0x%02X; check pull-ups", addr);
            return Result<void>::Err(AppError::I2C_BUS_INIT_FAILED, "I2C bus timeout");
        }
    }
    return Result<void>::Ok();
}

} // namespace hal
} // namespace smart_device

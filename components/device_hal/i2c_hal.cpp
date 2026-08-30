#include "i2c_hal.h"
#include "esp_log.h"

static const char* TAG = "I2C_HAL";

namespace smart_device {
namespace hal {

I2cHal& I2cHal::instance(){ static I2cHal s; return s; }

Result<void> I2cHal::initialize(const board::BoardConfig& cfg){
    if(initialized_) return Result<void>::Ok();
    i2c_config_t conf{};
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = static_cast<gpio_num_t>(cfg.i2c_sda);
    conf.scl_io_num = static_cast<gpio_num_t>(cfg.i2c_scl);
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = cfg.i2c_freq_hz;

    esp_err_t ret = i2c_param_config((i2c_port_t)cfg.i2c_port, &conf);
    if(ret!=ESP_OK) return Result<void>::Err(AppError::I2C_BUS_INIT_FAILED, esp_err_to_name(ret));
    ret = i2c_driver_install((i2c_port_t)cfg.i2c_port, conf.mode, 0, 0, 0);
    if(ret!=ESP_OK) return Result<void>::Err(AppError::I2C_BUS_INIT_FAILED, esp_err_to_name(ret));
    port_ = cfg.i2c_port;
    initialized_ = true;
    ESP_LOGI(TAG, "I2C initialized SDA=%d SCL=%d freq=%d port=%d", cfg.i2c_sda, cfg.i2c_scl, cfg.i2c_freq_hz, cfg.i2c_port);
    return Result<void>::Ok();
}

Result<void> I2cHal::scan(){
    ESP_LOGI(TAG, "I2C scan on port %d", port_);
    for(int addr=3; addr<0x78; ++addr){
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (addr<<1) | I2C_MASTER_WRITE, true);
        i2c_master_stop(cmd);
        esp_err_t ret = i2c_master_cmd_begin((i2c_port_t)port_, cmd, 100);
        i2c_cmd_link_delete(cmd);
        if(ret==ESP_OK) ESP_LOGI(TAG, "Found device at 0x%02X", addr);
    }
    return Result<void>::Ok();
}

} // namespace hal
} // namespace smart_device

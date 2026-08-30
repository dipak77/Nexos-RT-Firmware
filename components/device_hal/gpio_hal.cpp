#include "gpio_hal.h"
#include "driver/ledc.h"

namespace smart_device {
namespace hal {

Result<void> GpioHal::configure_output(gpio_num_t pin, bool initial) {
    gpio_config_t cfg{};
    cfg.pin_bit_mask = 1ULL << pin;
    cfg.mode = GPIO_MODE_OUTPUT;
    cfg.pull_up_en = GPIO_PULLUP_DISABLE;
    cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    cfg.intr_type = GPIO_INTR_DISABLE;
    if(gpio_config(&cfg)!=ESP_OK) return Result<void>::Err(AppError::SYSTEM_INVALID_STATE, "gpio config failed");
    gpio_set_level(pin, initial?1:0);
    return Result<void>::Ok();
}
Result<void> GpioHal::configure_input(gpio_num_t pin, gpio_pull_mode_t pull){
    gpio_config_t cfg{};
    cfg.pin_bit_mask = 1ULL << pin;
    cfg.mode = GPIO_MODE_INPUT;
    cfg.intr_type = GPIO_INTR_DISABLE;
    if(pull==GPIO_PULLUP_ONLY) cfg.pull_up_en=GPIO_PULLUP_ENABLE;
    else if(pull==GPIO_PULLDOWN_ONLY) cfg.pull_down_en=GPIO_PULLDOWN_ENABLE;
    if(gpio_config(&cfg)!=ESP_OK) return Result<void>::Err(AppError::SYSTEM_INVALID_STATE, "gpio input config");
    return Result<void>::Ok();
}
Result<void> GpioHal::set_level(gpio_num_t pin, bool level){
    if(gpio_set_level(pin, level?1:0)!=ESP_OK) return Result<void>::Err(AppError::SYSTEM_INVALID_STATE, "set_level");
    return Result<void>::Ok();
}
bool GpioHal::get_level(gpio_num_t pin){ return gpio_get_level(pin); }

Result<void> GpioHal::set_pwm(int pin, int duty_percent){
    if(pin<0) return Result<void>::Ok();
    // LEDC for backlight
    ledc_timer_config_t timer{};
    timer.speed_mode = LEDC_LOW_SPEED_MODE;
    timer.duty_resolution = LEDC_TIMER_10_BIT;
    timer.timer_num = LEDC_TIMER_0;
    timer.freq_hz = 5000;
    timer.clk_cfg = LEDC_AUTO_CLK;
    ledc_timer_config(&timer);
    ledc_channel_config_t ch{};
    ch.gpio_num = pin;
    ch.speed_mode = LEDC_LOW_SPEED_MODE;
    ch.channel = LEDC_CHANNEL_0;
    ch.timer_sel = LEDC_TIMER_0;
    ch.duty = (1023 * duty_percent)/100;
    ch.hpoint = 0;
    ledc_channel_config(&ch);
    return Result<void>::Ok();
}

} // namespace hal
} // namespace smart_device

#pragma once
#include "common/result.h"
#include "driver/gpio.h"

namespace smart_device {
namespace hal {

class GpioHal {
public:
    static Result<void> configure_output(gpio_num_t pin, bool initial_level = false);
    static Result<void> configure_input(gpio_num_t pin, gpio_pull_mode_t pull = GPIO_FLOATING);
    static Result<void> set_level(gpio_num_t pin, bool level);
    static bool get_level(gpio_num_t pin);
    static Result<void> set_pwm(int pin, int duty_percent); // for backlight
};

} // namespace hal
} // namespace smart_device

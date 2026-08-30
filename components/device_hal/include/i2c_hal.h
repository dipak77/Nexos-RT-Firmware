#pragma once
#include "common/result.h"
#include "driver/i2c.h"
#include "board/board_config.h"

namespace smart_device {
namespace hal {

class I2cHal {
public:
    static I2cHal& instance();
    Result<void> initialize(const board::BoardConfig& cfg);
    Result<void> scan();
    bool is_initialized() const { return initialized_; }
    i2c_port_t port() const { return (i2c_port_t)port_; }
private:
    bool initialized_{false};
    int port_{0};
};

} // namespace hal
} // namespace smart_device

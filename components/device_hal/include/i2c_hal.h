#pragma once
#include "common/result.h"
#include "driver/i2c_master.h"
#include "board/board_config.h"

namespace smart_device {
namespace hal {

class I2cHal {
public:
    static I2cHal& instance();
    Result<void> initialize(const board::BoardConfig& cfg);
    Result<void> scan();
    bool is_initialized() const { return initialized_; }
    int port() const { return port_; }
private:
    bool initialized_{false};
    int port_{0};
    i2c_master_bus_handle_t bus_{nullptr};
};

} // namespace hal
} // namespace smart_device

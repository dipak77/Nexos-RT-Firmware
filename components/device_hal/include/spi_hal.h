#pragma once
#include "common/result.h"
#include "driver/spi_master.h"
#include "board/board_config.h"

namespace smart_device {
namespace hal {

class SpiHal {
public:
    static SpiHal& instance();
    Result<void> initialize(const board::BoardConfig& cfg);
    spi_host_device_t host() const { return SPI2_HOST; }
    bool is_initialized() const { return initialized_; }
private:
    bool initialized_{false};
};

} // namespace hal
} // namespace smart_device

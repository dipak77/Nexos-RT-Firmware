#pragma once
#include "common/result.h"

namespace smart_device {
namespace hal {

class UsbHal {
public:
    static UsbHal& instance();
    Result<void> initialize_console(); // UART0 already, plus optional USB CDC
    Result<void> initialize_cdc(); // native USB CDC for field config
    bool is_cdc_ready() const { return cdc_ready_; }
private:
    bool cdc_ready_{false};
};

} // namespace hal
} // namespace smart_device

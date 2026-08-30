#pragma once
#include "common/result.h"

namespace smart_device {
namespace display {

class IDisplay {
public:
    virtual ~IDisplay() = default;
    virtual Result<void> init() = 0;
    virtual Result<void> clear(uint16_t color = 0x0000) = 0;
    virtual Result<void> set_brightness(uint8_t percent) = 0;
    virtual Result<void> test_pattern() = 0;
    virtual bool is_initialized() const = 0;
};

} // namespace display
} // namespace smart_device

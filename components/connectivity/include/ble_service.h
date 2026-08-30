#pragma once
#include "connection_state.h"
#include "common/result.h"

namespace smart_device {
namespace connectivity {

class BleService {
public:
    static BleService& instance();
    Result<void> initialize();
    Result<void> start_advertising();
    Result<void> stop_advertising();
    Result<void> stop();
    void set_connected(bool connected);

    BleStatus status() const { return status_; }
    bool is_advertising() const { return status_.advertising; }

private:
    BleService() = default;
    BleStatus status_{};
    bool initialized_{false};
};

} // namespace connectivity
} // namespace smart_device

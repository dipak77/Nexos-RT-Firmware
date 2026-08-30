#pragma once
#include "system_metrics.h"
#include "common/result.h"

namespace smart_device {
namespace diagnostics {

class HealthMonitor {
public:
    static HealthMonitor& instance();
    Result<void> initialize();
    SystemMetrics get_metrics();
    bool check_memory_leak();
    const char* reset_reason_str();

private:
    uint32_t last_free_heap_{0};
    bool initialized_{false};
};

} // namespace diagnostics
} // namespace smart_device

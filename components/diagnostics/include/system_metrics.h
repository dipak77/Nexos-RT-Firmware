#pragma once
#include <cstdint>

namespace smart_device {
namespace diagnostics {

struct SystemMetrics {
    uint32_t uptime_sec{0};
    uint32_t free_heap{0};
    uint32_t min_free_heap{0};
    uint32_t largest_free_block{0};
    int8_t internal_temp_c{0}; // if sensor available
    uint32_t task_count{0};
};

} // namespace diagnostics
} // namespace smart_device

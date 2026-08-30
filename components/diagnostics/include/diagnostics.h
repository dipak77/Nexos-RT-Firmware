#pragma once
#include "health_monitor.h"
#include "connectivity/connection_state.h"
#include "time_service/time_service.h"
#include <string>

namespace smart_device {
namespace diagnostics {

struct SelfTestResult {
    struct Test { const char* name; bool pass; const char* detail; };
    Test tests[16];
    int total{0};
    int passed{0};
    int failed{0};
    bool overall_pass{false};
};

class Diagnostics {
public:
    static Diagnostics& instance();
    void initialize();
    SelfTestResult run_self_test();
    std::string get_system_status_json();
    std::string get_system_status_text();
};

} // namespace diagnostics
} // namespace smart_device

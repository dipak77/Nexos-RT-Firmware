#include "health_monitor.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "mk.h"

static const char* TAG = "HEALTH_MON";

namespace smart_device {
namespace diagnostics {

HealthMonitor& HealthMonitor::instance(){ static HealthMonitor s; return s; }

Result<void> HealthMonitor::initialize(){
    if(initialized_) return Result<void>::Ok();
    last_free_heap_ = esp_get_free_heap_size();
    initialized_ = true;
    ESP_LOGI(TAG, "Health monitor init free_heap=%lu", (unsigned long)last_free_heap_);
    return Result<void>::Ok();
}

SystemMetrics HealthMonitor::get_metrics(){
    SystemMetrics m{};
    m.uptime_sec = esp_timer_get_time()/1000000;
    m.free_heap = esp_get_free_heap_size();
    m.min_free_heap = esp_get_minimum_free_heap_size();
    m.largest_free_block = heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT);
    m.task_count = mk_task_count();
    return m;
}

bool HealthMonitor::check_memory_leak(){
    uint32_t now = esp_get_free_heap_size();
    bool leak = (last_free_heap_ > now + 10*1024) && (now < 100*1024);
    last_free_heap_ = now;
    return !leak;
}

const char* HealthMonitor::reset_reason_str(){
    switch(esp_reset_reason()){
        case ESP_RST_POWERON: return "POWERON";
        case ESP_RST_EXT: return "EXTERNAL";
        case ESP_RST_SW: return "SOFTWARE";
        case ESP_RST_PANIC: return "PANIC";
        case ESP_RST_INT_WDT: return "INT_WDT";
        case ESP_RST_TASK_WDT: return "TASK_WDT";
        case ESP_RST_WDT: return "WDT";
        case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
        case ESP_RST_BROWNOUT: return "BROWNOUT";
        case ESP_RST_SDIO: return "SDIO";
        default: return "UNKNOWN";
    }
}

} // namespace diagnostics
} // namespace smart_device

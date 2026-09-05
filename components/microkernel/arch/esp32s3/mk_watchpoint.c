#include "mk_watchpoint.h"
#include "esp_cpu.h"
#include "esp_log.h"

static const char* TAG = "MK_WATCHPOINT";

bool mk_watchpoint_arm_stack_guard(int watchpoint_id, const void* stack_base, size_t size) {
    if (watchpoint_id < 0 || watchpoint_id > 1 || !stack_base) {
        return false;
    }

#if defined(ESP_PLATFORM)
    // ESP-IDF provides esp_cpu_set_watchpoint(int no, void *adr, int size, int flags)
    // flags: ESP_CPU_WATCHPOINT_STORE = 2, ESP_CPU_WATCHPOINT_LOAD = 1, BOTH = 3
    esp_err_t err = esp_cpu_set_watchpoint(watchpoint_id, (char*)stack_base, size, ESP_WATCHPOINT_STORE);
    if (err == ESP_OK) {
        ESP_LOGD(TAG, "Hardware stack guard armed at %p (size=%u) on WP %d", stack_base, (unsigned)size, watchpoint_id);
        return true;
    } else {
        ESP_LOGW(TAG, "Failed to arm hardware watchpoint %d: %d", watchpoint_id, err);
        return false;
    }
#else
    (void)size;
    return true;
#endif
}

void mk_watchpoint_disarm(int watchpoint_id) {
    if (watchpoint_id < 0 || watchpoint_id > 1) return;
#if defined(ESP_PLATFORM)
    esp_cpu_clear_watchpoint(watchpoint_id);
#endif
}

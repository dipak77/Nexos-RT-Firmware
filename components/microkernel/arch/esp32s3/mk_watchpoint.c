#include "mk_watchpoint.h"
#include "esp_cpu.h"
#include "esp_log.h"

static const char* TAG = "MK_WATCHPOINT";

// This Arduino core (IDF 5.x) defines only ESP_WATCHPOINT_*; newer IDFs renamed
// them ESP_CPU_WATCHPOINT_*. Verified against the bundled esp_cpu.h.
#ifndef ESP_WATCHPOINT_STORE
#ifdef ESP_CPU_WATCHPOINT_STORE
#define ESP_WATCHPOINT_STORE ESP_CPU_WATCHPOINT_STORE
#else
#error "No watchpoint STORE flag in this toolchain"
#endif
#endif

bool mk_watchpoint_arm_stack_guard(int watchpoint_id, const void* stack_base, size_t size) {
    if (watchpoint_id < 0 || watchpoint_id > 1 || !stack_base) {
        return false;
    }
    (void)size; // fixed 32-byte guard window (HW limit 64, alignment requirement)

#if defined(ESP_PLATFORM)
    // HW requires base naturally aligned to size (power of two, <= 64).
    // Best-effort only: a hit raises a CPU debug exception (panic), it is NOT
    // routed into the enclave fault path — so this is a tripwire, not isolation.
    uintptr_t aligned = (uintptr_t)stack_base & ~(uintptr_t)31;
    esp_err_t err = esp_cpu_set_watchpoint(watchpoint_id, (char*)aligned, 32, ESP_WATCHPOINT_STORE);
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

#include "mk_watchpoint.h"
#include "mk_port.h"
#include "esp_cpu.h"
#include "esp_log.h"

static const char* TAG = "MK_WATCHPOINT";

#ifdef ESP_WATCHPOINT_STORE
#define MK_WP_TRIGGER ESP_WATCHPOINT_STORE
#define esp_cpu_get_core_id() mk_port_get_core_id()
#else
#define MK_WP_TRIGGER ESP_CPU_WATCHPOINT_STORE
#endif

#if defined(ESP_PLATFORM)
#include "esp_ipc.h"

struct wp_arm_args {
    int id;
    void* addr;
    esp_err_t err;
};

static void ipc_arm_wp(void* arg){
    struct wp_arm_args* a = (struct wp_arm_args*)arg;
    a->err = esp_cpu_set_watchpoint(a->id, (char*)a->addr, 32, MK_WP_TRIGGER);
}

static void ipc_clear_wp(void* arg){
    int id = (int)(uintptr_t)arg;
    esp_cpu_clear_watchpoint(id);
}
#endif

bool mk_watchpoint_arm_stack_guard(int watchpoint_id, const void* stack_base, size_t size) {
    if (watchpoint_id < 0 || watchpoint_id > 1 || !stack_base) {
        return false;
    }
    (void)size; // fixed 32-byte guard window (HW limit 64, alignment requirement)

#if defined(ESP_PLATFORM)
    // HW requires base naturally aligned to size (power of two, <= 64).
    // Best-effort only: a hit raises a CPU debug exception (panic).
    uintptr_t aligned = (uintptr_t)stack_base & ~(uintptr_t)31;
    esp_err_t err = ESP_OK;

    // Enclaves run on Core 1: ensure watchpoint is set on Core 1
    if (esp_cpu_get_core_id() == 1) {
        err = esp_cpu_set_watchpoint(watchpoint_id, (char*)aligned, 32, MK_WP_TRIGGER);
    } else {
        struct wp_arm_args a = { watchpoint_id, (void*)aligned, ESP_OK };
        esp_ipc_call_blocking(1, ipc_arm_wp, &a);
        err = a.err;
    }

    if (err == ESP_OK) {
        ESP_LOGD(TAG, "Hardware stack guard armed at %p on Core 1 WP %d", stack_base, watchpoint_id);
        return true;
    } else {
        ESP_LOGW(TAG, "Failed to arm hardware watchpoint %d on Core 1: %d", watchpoint_id, err);
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
    if (esp_cpu_get_core_id() != 1) {
        // Enclaves run on Core 1: dispatch disarm to Core 1 so register is cleared there
        esp_ipc_call_blocking(1, ipc_clear_wp, (void*)(uintptr_t)watchpoint_id);
    }
#endif
}

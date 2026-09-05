#include "kernel_manager.h"
#include "mk_enclave.h"
#include "nvs_flash.h"
#include "esp_chip_info.h"
#include <cstring>

bool KernelManager::init() {
    if (initialized) return true;

    esp_err_t nvs = nvs_flash_init();
    if (nvs == ESP_ERR_NVS_NO_FREE_PAGES || nvs == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        esp_err_t er = nvs_flash_erase();
        if (er != ESP_OK) {
            Serial.printf("[WARN] NVS erase %s\n", esp_err_to_name(er));
        }
        nvs = nvs_flash_init();
    }
    if (nvs != ESP_OK) {
        Serial.printf("[WARN] NVS init %s — continuing degraded\n", esp_err_to_name(nvs));
    }

    mk_config_t cfg{};
    cfg.tick_hz = MK_CONFIG_TICK_HZ;
    cfg.use_preemption = true;
    cfg.single_core = false;
    cfg.version = MK_CONFIG_VERSION_STRING;
    if (mk_init(&cfg) != MK_OK) {
        Serial.printf("[FAIL] %s mk_init\n", MK_CONFIG_OS_NAME);
        return false;
    }
    mk_diagnostics_init();

    display_lock_ = mk_mutex_create("display");
    if (!display_lock_) {
        Serial.println("[FAIL] display mutex");
        return false;
    }

    initialized = true;
    Serial.printf("[KERNEL] %s ready.\n", MK_CONFIG_OS_NAME);
    Serial.printf("[KERNEL] %s v%s  tick=%uHz  app-core=1  max_tasks=%d\n",
                  MK_CONFIG_OS_NAME, MK_CONFIG_VERSION_STRING,
                  (unsigned)MK_CONFIG_TICK_HZ, MK_CONFIG_MAX_TASKS);
    return true;
}

// V2 binding: every boot task runs inside a capability enclave created first,
// then started via mk_enclave_start (which creates the task and attaches the
// descriptor for PI/SVC/budget attribution). Budgets stay 0 (dormant) for boot
// tasks: the 10Hz sampler enforces only enclaves with budget_us > 0, and the
// drill command proves the mechanism on a sacrificial enclave instead.
// Caps are least-privilege per role; base/limit are 0/0 (heap bounds unknown
// at boot — SVC pointer checks fall back to the task stack window).
mk_task_handle_t KernelManager::spawn(const char* name, uint8_t prio, size_t stack,
                                      int core, mk_task_entry_t entry,
                                      uint8_t enc_type, uint32_t enc_caps) {
    (void)core; // enclave_start pins app tasks to Core 1 (radio lives on Core 0)
    mk_enclave_desc_t* enc = mk_enclave_create(name, prio, enc_type,
                                               0, 0, stack, enc_caps, 0, 0);
    if (!enc) {
        Serial.printf("[FAIL] enclave %s (no free slots)\n", name);
        return nullptr;
    }
    if (mk_enclave_start(enc, entry, nullptr) != MK_OK || !enc->task_handle) {
        Serial.printf("[FAIL] spawn %s prio=%u (enclave start failed)\n", name, (unsigned)prio);
        mk_enclave_reclaim(enc);
        return nullptr;
    }
    mk_task_handle_t h = enc->task_handle;
    if (!strcmp(name, "GUI")) gui_enc_ = enc;
    else if (!strcmp(name, "SYSTEM")) sys_enc_ = enc;
    else if (!strcmp(name, "CLI")) cli_enc_ = enc;
    mk_watchdog_register_task(h, 4000, nullptr);
    Serial.printf("[PASS] %s task %s  prio=%u core=1 stack=%u enc=%lu caps=0x%08lX\n",
                  MK_CONFIG_OS_NAME, name, (unsigned)prio, (unsigned)stack,
                  (unsigned long)enc->id, (unsigned long)enc->syscall_mask);
    return h;
}

void KernelManager::rollback_one(const char* wdt_name, mk_task_handle_t& task, mk_enclave_desc_t*& enc) {
    if (!task && !enc) return;
    mk_watchdog_deregister(wdt_name);
    if (enc) {
        // Reclaim deletes the bound task, sanitizes the stack, frees the slot.
        mk_enclave_reclaim(enc);
        enc = nullptr;
    } else if (task) {
        mk_task_delete(task); // legacy fallback: unbound task
    }
    task = nullptr;
}

void KernelManager::rollback() {
    rollback_one("CLI", cli_, cli_enc_);
    rollback_one("SYSTEM", sys_, sys_enc_);
    rollback_one("GUI", gui_, gui_enc_);
}

bool KernelManager::startTasks(mk_task_entry_t gui, mk_task_entry_t sys, mk_task_entry_t cli) {
    if (!initialized && !init()) return false;

    gui_ = spawn("GUI", MK_PRIO_GUI, 8192, 1, gui, MK_ENCLAVE_TYPE_RT, MK_CAP_GFX);
    if (!gui_) return false;
    // Wi-Fi AP + BLE init run on SYSTEM; 4 KB overflows and resets the chip.
    sys_ = spawn("SYSTEM", MK_PRIO_DIAGNOSTICS, 12288, 1, sys,
                 MK_ENCLAVE_TYPE_RT, (MK_CAP_NET | MK_CAP_SYSTEM | MK_CAP_GPIO));
    if (!sys_) { rollback(); return false; }
    cli_ = spawn("CLI", MK_PRIO_COMMAND, 6144, 1, cli, MK_ENCLAVE_TYPE_RT, MK_CAP_SYSTEM);
    if (!cli_) { rollback(); return false; }

    if (mk_start() != MK_OK) {
        Serial.printf("[FAIL] %s mk_start\n", MK_CONFIG_OS_NAME);
        rollback();
        return false;
    }
    Serial.printf("[KERNEL] %s running — %lu tasks\n",
                  MK_CONFIG_OS_NAME, (unsigned long)mk_task_count());
    return true;
}

void KernelManager::delayMs(uint32_t ms) const {
    mk_sleep_ms(ms ? ms : 1);
}

void KernelManager::heartbeat(const char* task_name) {
    mk_watchdog_feed(task_name);
}

int KernelManager::currentCore() const {
    return mk_current_core();
}

bool KernelManager::lockDisplay(uint32_t timeout_ms) {
    if (!display_lock_) return false;
    return mk_mutex_lock(display_lock_, timeout_ms) == MK_OK;
}

bool KernelManager::lockDisplayRetry(uint32_t timeout_ms, int tries) {
    for (int i = 0; i < tries; i++) {
        if (lockDisplay(timeout_ms)) return true;
        mk_watchdog_feed_self();
        delayMs(15);
    }
    return false;
}

void KernelManager::unlockDisplay() {
    if (display_lock_) mk_mutex_unlock(display_lock_);
}

KernelStats KernelManager::getStats() {
    mk_diagnostics_tick();
    mk_kernel_stats_t ks = mk_kernel_get_stats();
    KernelStats stats{};
    stats.os_name = MK_CONFIG_OS_NAME;
    stats.version = MK_CONFIG_VERSION_STRING;
    stats.uptime_seconds = (uint32_t)(mk_time_ms() / 1000ULL);
    stats.free_heap_kb = (uint32_t)(ks.free_heap / 1024);
    stats.min_free_heap_kb = (uint32_t)(ks.min_free_heap / 1024);
    stats.psram_size_kb = ESP.getPsramSize() / 1024;
    stats.free_psram_kb = ESP.getFreePsram() / 1024;
    esp_chip_info_t chip{};
    esp_chip_info(&chip);
    stats.core_count = chip.cores;
    stats.active_tasks_count = (uint8_t)mk_task_count();
    stats.is_preemptive = true;
    stats.healthy = mk_diagnostics_healthy();
    mk_diagnostics_copy_health(stats.health_text, sizeof(stats.health_text));
    return stats;
}

void KernelManager::printKernelInfo() {
    KernelStats stats = getStats();
    Serial.println("\n================ NEXOS-RT ================");
    Serial.printf(" OS                : %s\n", stats.os_name);
    Serial.printf(" Version           : V%s\n", stats.version);
    Serial.printf(" State             : %s\n", mk_is_running() ? "RUNNING" : "INIT");
    Serial.printf(" Health            : %s\n", stats.health_text);
    Serial.printf(" App core          : 1    Sys domain : 0\n");
    Serial.printf(" CPU               : %d x Xtensa LX7 @ 240 MHz\n", stats.core_count);
    Serial.printf(" Tasks             : %d / %d\n", stats.active_tasks_count, MK_CONFIG_MAX_TASKS);
    if (gui_) Serial.printf("   GUI             : id=%lu prio=%u\n",
                            (unsigned long)gui_->id, (unsigned)gui_->priority);
    if (sys_) Serial.printf("   SYSTEM          : id=%lu prio=%u\n",
                            (unsigned long)sys_->id, (unsigned)sys_->priority);
    if (cli_) Serial.printf("   CLI             : id=%lu prio=%u\n",
                            (unsigned long)cli_->id, (unsigned)cli_->priority);
    Serial.printf(" Watchdog GUI age  : %lu ms\n", (unsigned long)mk_watchdog_age_ms("GUI"));
    Serial.printf(" Watchdog SYS age  : %lu ms\n", (unsigned long)mk_watchdog_age_ms("SYSTEM"));
    Serial.printf(" Watchdog CLI age  : %lu ms\n", (unsigned long)mk_watchdog_age_ms("CLI"));
    Serial.printf(" Free SRAM         : %u KB (min %u KB)\n", stats.free_heap_kb, stats.min_free_heap_kb);
    Serial.printf(" Free PSRAM        : %u KB / %u KB\n", stats.free_psram_kb, stats.psram_size_kb);
    Serial.printf(" Uptime            : %lu s\n", (unsigned long)stats.uptime_seconds);
    Serial.println("==========================================");
}

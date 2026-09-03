#include "mk_diagnostics.h"
#include "mk_kernel.h"
#include "mk_task.h"
#include "mk_health.h"
#include "mk_crash.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "mk_port.h"
#include <stdlib.h>
#include <string.h>

static const char* TAG = "NEXTOS_DIAG";

typedef struct {
    char name[16];
    uint32_t timeout_ms;
    uint64_t last_feed_us;
    bool registered;
    bool seen_feed;
    bool stalled;
    mk_watchdog_callback_t cb;
} mk_wd_t;

static mk_wd_t s_wd[MK_DIAG_MAX_WATCHDOGS];
static bool s_init;
static bool s_healthy = true;
static char s_health[24] = "OK";
static uint64_t s_last_idle_us;
static uint32_t s_cpu_load;
static bool s_tick_running;

static void copy_bounded(char* dst, size_t dst_len, const char* src) {
    if (!dst || dst_len == 0) return;
    if (!src) src = "";
    size_t count = strnlen(src, dst_len - 1);
    memcpy(dst, src, count);
    dst[count] = 0;
}


static uint64_t now_us(void) { return (uint64_t)esp_timer_get_time(); }

mk_status_t mk_diagnostics_init(void) {
    memset(s_wd, 0, sizeof(s_wd));
    s_init = true;
    s_healthy = true;
    copy_bounded(s_health, sizeof(s_health), "OK");
    s_last_idle_us = now_us();
    s_cpu_load = 0;
    s_tick_running = false;
    ESP_LOGI(TAG, "%s diagnostics ready (watchdog slots=%d heap_warn=%dKB)",
             MK_CONFIG_OS_NAME, MK_DIAG_MAX_WATCHDOGS, MK_DIAG_HEAP_WARN_BYTES / 1024);
    return MK_OK;
}

// SMP-protected lookup — every s_wd[] access must hold s_diag_lock
static mk_wd_t* find_wd_locked(const char* name, bool alloc) {
    if (!name) return NULL;
    int empty = -1;
    for (int i = 0; i < MK_DIAG_MAX_WATCHDOGS; i++) {
        if (s_wd[i].registered && strncmp(s_wd[i].name, name, sizeof(s_wd[i].name)) == 0) {
            return &s_wd[i];
        }
        if (!s_wd[i].registered && empty < 0) empty = i;
    }
    if (!alloc || empty < 0) return NULL;
    mk_wd_t* w = &s_wd[empty];
    memset(w, 0, sizeof(*w));
    copy_bounded(w->name, sizeof(w->name), name);
    w->registered = true;
    return w;
}
void mk_watchdog_register(const char* name, uint32_t timeout_ms, mk_watchdog_callback_t cb) {
    if (!s_init) mk_diagnostics_init();
    mk_port_enter_critical();
    mk_wd_t* w = find_wd_locked(name, true);
    if (!w) {
        mk_port_exit_critical();
        ESP_LOGW(TAG, "watchdog table full, skip %s", name ? name : "?");
        return;
    }
    w->timeout_ms = timeout_ms ? timeout_ms : MK_WATCHDOG_TIMEOUT_MS;
    w->cb = cb;
    w->last_feed_us = now_us();
    w->seen_feed = false;
    w->stalled = false;
    char registered_name[sizeof(w->name)];
    copy_bounded(registered_name, sizeof(registered_name), w->name);
    uint32_t registered_timeout = w->timeout_ms;
    mk_port_exit_critical();
    ESP_LOGI(TAG, "watchdog %s timeout=%lums", registered_name,
             (unsigned long)registered_timeout);
}

void mk_watchdog_feed(const char* name) {
    mk_port_enter_critical();
    mk_wd_t* w = find_wd_locked(name, false);
    if (!w) { mk_port_exit_critical(); return; }
    w->last_feed_us = now_us();
    w->seen_feed = true;
    w->stalled = false;
    mk_port_exit_critical();
}

mk_watchdog_token_t mk_watchdog_register_task(mk_task_handle_t task, uint32_t timeout_ms, mk_watchdog_callback_t cb) {
    const char* name = mk_task_get_name(task);
    mk_watchdog_register(name, timeout_ms, cb);
    mk_port_enter_critical();
    mk_wd_t* w = find_wd_locked(name, false);
    mk_watchdog_token_t tok = 0;
    if (w) tok = (mk_watchdog_token_t)((w - s_wd) + 1);
    mk_port_exit_critical();
    return tok;
}

void mk_watchdog_feed_token(mk_watchdog_token_t token) {
    if (token == 0 || token > MK_DIAG_MAX_WATCHDOGS) return;
    mk_port_enter_critical();
    mk_wd_t* w = &s_wd[token - 1];
    if (w->registered) {
        w->last_feed_us = now_us();
        w->seen_feed = true;
        w->stalled = false;
    }
    mk_port_exit_critical();
}

void mk_watchdog_deregister(const char* name) {
    mk_port_enter_critical();
    mk_wd_t* w = find_wd_locked(name, false);
    if (w) {
        memset(w, 0, sizeof(*w));
    }
    mk_port_exit_critical();
}

void mk_watchdog_feed_self(void) {
    mk_task_handle_t self = mk_task_self();
    if (self && self->name) mk_watchdog_feed(self->name);
}

void mk_diagnostics_copy_health(char* dst, size_t dst_len) {
    if (!dst || dst_len == 0) return;
    mk_port_enter_critical();
    copy_bounded(dst, dst_len, s_health);
    mk_port_exit_critical();
}

uint32_t mk_watchdog_age_ms(const char* name) {
    mk_port_enter_critical();
    mk_wd_t* w = find_wd_locked(name, false);
    uint32_t age = 0;
    if (w && w->seen_feed) age = (uint32_t)((now_us() - w->last_feed_us) / 1000ULL);
    mk_port_exit_critical();
    return age;
}

void mk_diagnostics_tick(void) {
    if (!s_init) return;

    // GUI, system and CLI tasks all request snapshots. Serialize the heavier
    // diagnostics pass without holding the SMP spinlock across heap/runtime
    // APIs or user callbacks.
    mk_port_enter_critical();
    if (s_tick_running) {
        mk_port_exit_critical();
        return;
    }
    s_tick_running = true;
    mk_port_exit_critical();

    uint64_t t = now_us();
    bool ok = true;
    const char* text = "OK";

    size_t free_heap = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    if (free_heap < MK_DIAG_HEAP_WARN_BYTES) {
        ok = false;
        text = "LOW HEAP";
    }

    typedef struct {
        char name[16];
        uint32_t age_ms;
        uint32_t timeout_ms;
        mk_watchdog_callback_t cb;
    } stall_notice_t;
    stall_notice_t notices[MK_DIAG_MAX_WATCHDOGS];
    size_t notice_count = 0;

    mk_port_enter_critical();
    for (int i = 0; i < MK_DIAG_MAX_WATCHDOGS; i++) {
        mk_wd_t* w = &s_wd[i];
        if (!w->registered || !w->seen_feed) continue;
        uint32_t age = (uint32_t)((t - w->last_feed_us) / 1000ULL);
        if (age > w->timeout_ms) {
            if (!w->stalled && notice_count < MK_DIAG_MAX_WATCHDOGS) {
                stall_notice_t* n = &notices[notice_count++];
                copy_bounded(n->name, sizeof(n->name), w->name);
                n->age_ms = age;
                n->timeout_ms = w->timeout_ms;
                n->cb = w->cb;
            }
            w->stalled = true;
            ok = false;
            if (strcmp(w->name, "GUI") == 0) text = "GUI STALL";
            else if (strcmp(w->name, "SYSTEM") == 0) text = "SYS STALL";
            else if (strcmp(w->name, "CLI") == 0) text = "CLI STALL";
            else text = "TASK STALL";
        }
    }
    mk_port_exit_critical();

    for (size_t i = 0; i < notice_count; ++i) {
        ESP_LOGW(TAG, "STALL %s age=%lums timeout=%lums", notices[i].name,
                 (unsigned long)notices[i].age_ms,
                 (unsigned long)notices[i].timeout_ms);
        mk_health_record_fault("STALL", 20);
        mk_crash_save("STALL", notices[i].name, 0, "Watchdog deadline exceeded");
        if (notices[i].cb) notices[i].cb(notices[i].name);
    }
    mk_health_tick();

    // Real CPU load: 100 * (1 - idle_runtime / total_runtime) using FreeRTOS run-time counters.
    // Requires CONFIG_FREERTOS_USE_TRACE_FACILITY + CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS.
    // If runtime counters are unavailable, report 0 rather than fabricating a
    // plausible-looking load value.
#if defined(CONFIG_FREERTOS_USE_TRACE_FACILITY) && defined(CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS)
    {
        static uint32_t s_prev_idle = 0, s_prev_total = 0;
        UBaseType_t n = uxTaskGetNumberOfTasks();
        TaskStatus_t* arr = (TaskStatus_t*)malloc(n * sizeof(TaskStatus_t));
        if (arr) {
            UBaseType_t got = uxTaskGetSystemState(arr, n, NULL);
            uint32_t total = 0, idle = 0;
            for (UBaseType_t i = 0; i < got; i++) {
                total += arr[i].ulRunTimeCounter;
                if (strncmp(arr[i].pcTaskName, "IDLE", 4) == 0) idle += arr[i].ulRunTimeCounter;
            }
            free(arr);
            if (total > s_prev_total && s_prev_total != 0) {
                uint32_t d_total = total - s_prev_total;
                uint32_t d_idle = (idle >= s_prev_idle) ? (idle - s_prev_idle) : 0;
                uint32_t load = 0;
                if (d_total) load = 100 - (d_idle * 100u / d_total);
                if (load > 100) load = 100;
                s_cpu_load = (s_cpu_load * 3u + load) / 4u;
            }
            s_prev_total = total;
            s_prev_idle = idle;
            s_last_idle_us = t;
        } else {
            uint64_t dt = t - s_last_idle_us;
            if (dt > 1000000ULL) s_last_idle_us = t;
        }
    }
#else
    {
        uint64_t dt = t - s_last_idle_us;
        if (dt > 1000000ULL) {
            s_cpu_load = 0;
            s_last_idle_us = t;
        }
    }
#endif

    mk_port_enter_critical();
    s_healthy = ok;
    copy_bounded(s_health, sizeof(s_health), text);
    s_tick_running = false;
    mk_port_exit_critical();
}

bool mk_diagnostics_healthy(void) {
    mk_port_enter_critical();
    bool healthy = s_healthy;
    mk_port_exit_critical();
    return healthy;
}
const char* mk_diagnostics_health_text(void) { return s_health; }

uint32_t mk_task_get_stack_high_watermark(mk_task_handle_t task) {
    mk_task_info_t info;
    if (mk_task_get_info(task, &info) != MK_OK) return 0;
    return info.stack_high_watermark;
}

uint64_t mk_task_get_runtime(mk_task_handle_t task) {
    mk_task_info_t info;
    if (mk_task_get_info(task, &info) != MK_OK) return 0;
    return info.runtime_ticks;
}

uint32_t mk_kernel_get_cpu_load(void) {
    mk_port_enter_critical();
    uint32_t load = s_cpu_load;
    mk_port_exit_critical();
    return load;
}

void mk_kernel_print_stats(void) {
    mk_kernel_stats_t s = mk_kernel_get_stats();
    char health[sizeof(s_health)];
    mk_diagnostics_copy_health(health, sizeof(health));
    ESP_LOGI(TAG, "%s v%s running=%d tasks=%lu heap=%luKB health=%s",
             MK_CONFIG_OS_NAME, MK_CONFIG_VERSION_STRING, mk_is_running() ? 1 : 0,
             (unsigned long)s.total_tasks, (unsigned long)(s.free_heap / 1024),
             health);
}

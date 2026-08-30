#include "mk_diagnostics.h"
#include "mk_kernel.h"
#include "mk_task.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "mk_chip_port.h"
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
static portMUX_TYPE s_diag_lock = portMUX_INITIALIZER_UNLOCKED;

static uint64_t now_us(void) { return (uint64_t)esp_timer_get_time(); }

mk_status_t mk_diagnostics_init(void) {
    memset(s_wd, 0, sizeof(s_wd));
    s_init = true;
    s_healthy = true;
    strncpy(s_health, "OK", sizeof(s_health) - 1);
    s_last_idle_us = now_us();
    s_cpu_load = 0;
    ESP_LOGI(TAG, "%s diagnostics ready (watchdog slots=%d heap_warn=%dKB)",
             MK_CONFIG_OS_NAME, MK_DIAG_MAX_WATCHDOGS, MK_DIAG_HEAP_WARN_BYTES / 1024);
    return MK_OK;
}

static mk_wd_t* find_wd(const char* name, bool alloc) {
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
    strncpy(w->name, name, sizeof(w->name) - 1);
    w->registered = true;
    return w;
}

void mk_watchdog_register(const char* name, uint32_t timeout_ms, mk_watchdog_callback_t cb) {
    if (!s_init) mk_diagnostics_init();
    mk_wd_t* w = find_wd(name, true);
    if (!w) {
        ESP_LOGW(TAG, "watchdog table full, skip %s", name ? name : "?");
        return;
    }
    w->timeout_ms = timeout_ms ? timeout_ms : MK_WATCHDOG_TIMEOUT_MS;
    w->cb = cb;
    w->last_feed_us = now_us();
    w->seen_feed = false;
    w->stalled = false;
    ESP_LOGI(TAG, "watchdog %s timeout=%lums", w->name, (unsigned long)w->timeout_ms);
}

void mk_watchdog_feed(const char* name) {
    mk_wd_t* w = find_wd(name, false);
    if (!w) return;
    taskENTER_CRITICAL(&s_diag_lock);
    w->last_feed_us = now_us();
    w->seen_feed = true;
    w->stalled = false;
    taskEXIT_CRITICAL(&s_diag_lock);
}

mk_watchdog_token_t mk_watchdog_register_task(mk_task_handle_t task, uint32_t timeout_ms, mk_watchdog_callback_t cb) {
    const char* name = mk_task_get_name(task);
    mk_watchdog_register(name, timeout_ms, cb);
    mk_wd_t* w = find_wd(name, false);
    if (!w) return 0;
    return (mk_watchdog_token_t)((w - s_wd) + 1);
}

void mk_watchdog_feed_token(mk_watchdog_token_t token) {
    if (token == 0 || token > MK_DIAG_MAX_WATCHDOGS) return;
    taskENTER_CRITICAL(&s_diag_lock);
    mk_wd_t* w = &s_wd[token - 1];
    if (w->registered) {
        w->last_feed_us = now_us();
        w->seen_feed = true;
        w->stalled = false;
    }
    taskEXIT_CRITICAL(&s_diag_lock);
}

void mk_watchdog_feed_self(void) {
    mk_task_handle_t self = mk_task_self();
    if (self && self->name) mk_watchdog_feed(self->name);
}

void mk_diagnostics_copy_health(char* dst, size_t dst_len) {
    if (!dst || dst_len == 0) return;
    taskENTER_CRITICAL(&s_diag_lock);
    strncpy(dst, s_health, dst_len - 1);
    taskEXIT_CRITICAL(&s_diag_lock);
    dst[dst_len - 1] = 0;
}

uint32_t mk_watchdog_age_ms(const char* name) {
    mk_wd_t* w = find_wd(name, false);
    if (!w || !w->seen_feed) return 0;
    return (uint32_t)((now_us() - w->last_feed_us) / 1000ULL);
}

void mk_diagnostics_tick(void) {
    if (!s_init) return;
    uint64_t t = now_us();
    bool ok = true;
    const char* text = "OK";

    size_t free_heap = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    if (free_heap < MK_DIAG_HEAP_WARN_BYTES) {
        ok = false;
        text = "LOW HEAP";
    }

    for (int i = 0; i < MK_DIAG_MAX_WATCHDOGS; i++) {
        mk_wd_t* w = &s_wd[i];
        if (!w->registered || !w->seen_feed) continue;
        uint32_t age = (uint32_t)((t - w->last_feed_us) / 1000ULL);
        if (age > w->timeout_ms) {
            if (!w->stalled) {
                ESP_LOGW(TAG, "STALL %s age=%lums timeout=%lums",
                         w->name, (unsigned long)age, (unsigned long)w->timeout_ms);
                if (w->cb) w->cb(w->name);
            }
            w->stalled = true;
            ok = false;
            if (strcmp(w->name, "GUI") == 0) text = "GUI STALL";
            else if (strcmp(w->name, "SYSTEM") == 0) text = "SYS STALL";
            else if (strcmp(w->name, "CLI") == 0) text = "CLI STALL";
            else text = "TASK STALL";
        }
    }

    uint64_t dt = t - s_last_idle_us;
    if (dt > 1000000ULL) {
        s_cpu_load = (s_cpu_load * 3u + (mk_task_count() * 12u)) / 4u;
        if (s_cpu_load > 100) s_cpu_load = 100;
        s_last_idle_us = t;
    }

    taskENTER_CRITICAL(&s_diag_lock);
    s_healthy = ok;
    strncpy(s_health, text, sizeof(s_health) - 1);
    s_health[sizeof(s_health) - 1] = 0;
    taskEXIT_CRITICAL(&s_diag_lock);
}

bool mk_diagnostics_healthy(void) { return s_healthy; }
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

uint32_t mk_kernel_get_cpu_load(void) { return s_cpu_load; }

void mk_kernel_print_stats(void) {
    mk_kernel_stats_t s = mk_kernel_get_stats();
    ESP_LOGI(TAG, "%s v%s running=%d tasks=%lu heap=%luKB health=%s",
             MK_CONFIG_OS_NAME, MK_CONFIG_VERSION_STRING, mk_is_running() ? 1 : 0,
             (unsigned long)s.total_tasks, (unsigned long)(s.free_heap / 1024),
             s_health);
}

#include "mk_gptimer.h"
#include "mk_scheduler.h"
#include "esp_timer.h"
#include "esp_log.h"

static const char* TAG = "MK_GPTIMER";
static bool s_running = false;

// Prod-grade: SINGLE tick source. Despite the filename, this layer uses the
// scheduler's esp_timer 1kHz driver (no separate HW GPTimer alarm is claimed
// from Arduino). This module is an idempotent alias: starting both would
// double-tick (sleep queue 2x fast, jitter stat wrong), so start/stop simply
// delegate and are safe to call alongside mk_scheduler_start/stop_tick().

bool mk_gptimer_init(void) {
    ESP_LOGI(TAG, "Tick alias ready (single source: scheduler 1kHz driver)");
    return true;
}

void mk_gptimer_start(void) {
    if (!s_running) {
        mk_scheduler_start_tick(); // idempotent inside
        s_running = true;
        ESP_LOGI(TAG, "Tick started via scheduler driver (1000us period)");
    }
}

void mk_gptimer_stop(void) {
    if (s_running) {
        mk_scheduler_stop_tick();
        s_running = false;
        ESP_LOGI(TAG, "Tick stopped");
    }
}

uint64_t mk_gptimer_get_time_us(void) {
    return (uint64_t)esp_timer_get_time();
}

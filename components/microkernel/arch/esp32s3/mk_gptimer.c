#include "mk_gptimer.h"
#include "mk_scheduler.h"
#include "esp_timer.h"
#include "esp_log.h"

static const char* TAG = "MK_GPTIMER";
static esp_timer_handle_t s_nexos_hw_timer = NULL;
static bool s_running = false;

static void IRAM_ATTR nexos_timer_callback(void* arg) {
    (void)arg;
    // Direct 1kHz kernel tick invocation
    mk_scheduler_tick(0);
}

bool mk_gptimer_init(void) {
    if (s_nexos_hw_timer != NULL) {
        return true;
    }

    const esp_timer_create_args_t timer_args = {
        .callback = &nexos_timer_callback,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "mk_1khz_tick",
        .skip_unhandled_events = true
    };

    esp_err_t err = esp_timer_create(&timer_args, &s_nexos_hw_timer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create hardware tick timer: %d", err);
        return false;
    }

    ESP_LOGI(TAG, "Nexos-RT 1kHz Hardware Preemption Timer initialized");
    return true;
}

void mk_gptimer_start(void) {
    if (s_nexos_hw_timer && !s_running) {
        esp_timer_start_periodic(s_nexos_hw_timer, 1000); // 1000us = 1ms = 1kHz
        s_running = true;
        ESP_LOGI(TAG, "Hardware tick timer started (1000us period)");
    }
}

void mk_gptimer_stop(void) {
    if (s_nexos_hw_timer && s_running) {
        esp_timer_stop(s_nexos_hw_timer);
        s_running = false;
        ESP_LOGI(TAG, "Hardware tick timer stopped");
    }
}

uint64_t mk_gptimer_get_time_us(void) {
    return (uint64_t)esp_timer_get_time();
}

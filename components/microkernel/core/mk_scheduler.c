#include "mk_scheduler.h"
#include "esp_log.h"

static const char* TAG = "MK_SCHED";
// Architecture A: Nexos-RT is a runtime over FreeRTOS — FreeRTOS does the real scheduling.
// This module keeps ONLY diagnostics counters (context_switches, tick) — no shadow ready-queues.
// The previous s_ready_queues[] was a fake that confused P0-C7; removed. mk_task.c no longer pretends to schedule.
static uint64_t s_context_switches = 0;

void mk_scheduler_init(void){
    s_context_switches=0;
    ESP_LOGI(TAG, "Nexos-RT scheduler (diagnostics-only) init max_tasks=%d tick=%dHz", MK_CONFIG_MAX_TASKS, MK_CONFIG_TICK_HZ);
}
void mk_scheduler_tick(uint64_t now_ms){
    (void)now_ms;
    s_context_switches++;
}
mk_task_t* mk_scheduler_pick_next(void){
    // Not used — FreeRTOS picks. Kept for API compat, returns NULL.
    return NULL;
}
void mk_scheduler_add_ready(mk_task_t* task){
    (void)task;
    // No-op: real ready-list is FreeRTOS. Keep counter bump for diagnostics parity.
    s_context_switches++;
}
void mk_scheduler_remove_ready(mk_task_t* task){
    (void)task;
    // No-op
}
uint64_t mk_scheduler_get_context_switches(void){ return s_context_switches; }

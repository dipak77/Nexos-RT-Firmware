#pragma once
#include "mk_types.h"
#ifdef __cplusplus
extern "C" {
#endif
// Real O(1) Preemptive Bitmap Scheduler
void mk_scheduler_init(void);
void mk_scheduler_tick(uint64_t now_ms);
mk_task_t* mk_scheduler_pick_next(void);
void mk_scheduler_add_ready(mk_task_t* task);
void mk_scheduler_remove_ready(mk_task_t* task);
void mk_scheduler_sleep_task(mk_task_t* task, uint32_t ms);
void mk_scheduler_wake_task(mk_task_t* task);
void mk_scheduler_block_task(mk_task_t* task);
void mk_scheduler_unblock_task(mk_task_t* task);
uint32_t mk_scheduler_ready_count(void);
uint32_t mk_scheduler_get_max_jitter_us(void);
uint64_t mk_scheduler_get_context_switches(void);
mk_task_t* mk_scheduler_current_task(void);
void mk_scheduler_set_current_task(mk_task_t* task);
// 1kHz accounting tick driver (esp_timer). Makes quantum/sleep/jitter real
// even though preemption itself remains FreeRTOS-driven (hybrid model).
void mk_scheduler_start_tick(void);
void mk_scheduler_stop_tick(void);
#ifdef __cplusplus
}
#endif

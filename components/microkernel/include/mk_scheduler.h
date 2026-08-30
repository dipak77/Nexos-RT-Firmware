#pragma once
#include "mk_types.h"
#ifdef __cplusplus
extern "C" {
#endif
// Internal scheduler - not for app use, but exposed for diagnostics
void mk_scheduler_init(void);
void mk_scheduler_tick(uint64_t now_ms);
mk_task_t* mk_scheduler_pick_next(void);
void mk_scheduler_add_ready(mk_task_t* task);
void mk_scheduler_remove_ready(mk_task_t* task);
uint64_t mk_scheduler_get_context_switches(void);
#ifdef __cplusplus
}
#endif

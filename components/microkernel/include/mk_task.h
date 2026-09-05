#pragma once
#include "mk_types.h"
#ifdef __cplusplus
extern "C" {
#endif

mk_task_handle_t mk_task_create(const char* name, mk_task_entry_t entry, void* arg, void* stack, size_t stack_size, uint8_t priority);
mk_task_handle_t mk_task_create_ext(const mk_task_config_t* config, mk_task_entry_t entry, void* arg);
mk_status_t mk_task_delete(mk_task_handle_t task);
mk_status_t mk_task_suspend(mk_task_handle_t task);
mk_status_t mk_task_resume(mk_task_handle_t task);
mk_task_handle_t mk_task_self(void);
// Backend port handle for a LIVE task (run-time-stat matching, debug).
// NULL when the task is not live. Never dereference the result.
void* mk_task_get_port_handle(mk_task_t* task);
const char* mk_task_get_name(mk_task_handle_t task);
mk_task_state_t mk_task_get_state(mk_task_handle_t task);
mk_status_t mk_task_get_info(mk_task_handle_t task, mk_task_info_t* info);
uint32_t mk_task_count(void);

#ifdef __cplusplus
}
#endif

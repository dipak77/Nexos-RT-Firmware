#pragma once
// Nexos-RT Port Abstraction — the ONLY file that may touch vendor scheduler.
// Core (mk_task, mk_mutex, ...) includes this, never freertos/*.h directly.
// Selected by MK_NATIVE_KERNEL in mk_config.h.
#include "mk_config.h"

#if MK_NATIVE_KERNEL
  // Native port: no FreeRTOS headers exposed to kernel core
  #include "native/mk_port_native_defs.h"
#else
  // Legacy shim port: keep FreeRTOS isolated here
  #include "freertos/mk_port_freertos_defs.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Common port API — both backends implement these
void mk_port_enter_critical(void);
void mk_port_exit_critical(void);
void mk_port_yield(void);
void mk_port_delay_ms(uint32_t ms);
uint64_t mk_port_get_time_us(void);
int mk_port_get_core_id(void);

// Task port
typedef void (*mk_port_task_entry_t)(void* arg);
typedef void* mk_port_task_handle_t;
mk_port_task_handle_t mk_port_task_create(const char* name, mk_port_task_entry_t entry, void* arg,
                                          uint32_t stack_bytes, uint8_t mk_prio, int core_affinity);
void mk_port_task_delete(mk_port_task_handle_t h);
mk_port_task_handle_t mk_port_task_self(void);
uint32_t mk_port_task_get_stack_watermark(mk_port_task_handle_t h);
void mk_port_task_suspend(mk_port_task_handle_t h);
void mk_port_task_resume(mk_port_task_handle_t h);

// Event port
typedef void* mk_port_event_group_handle_t;
mk_port_event_group_handle_t mk_port_event_create(const char* name);
void mk_port_event_delete(mk_port_event_group_handle_t h);
uint32_t mk_port_event_set(mk_port_event_group_handle_t h, uint32_t bits);
uint32_t mk_port_event_clear(mk_port_event_group_handle_t h, uint32_t bits);
uint32_t mk_port_event_wait(mk_port_event_group_handle_t h, uint32_t bits, bool clear_on_exit, bool wait_all, uint32_t timeout_ms);
uint32_t mk_port_event_get(mk_port_event_group_handle_t h);

#ifdef __cplusplus
}
#endif

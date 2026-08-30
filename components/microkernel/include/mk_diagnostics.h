#pragma once
#include "mk_types.h"
#ifdef __cplusplus
extern "C" {
#endif

#define MK_DIAG_MAX_WATCHDOGS 8
#define MK_DIAG_HEAP_WARN_BYTES (40 * 1024)

typedef uint32_t mk_watchdog_token_t;
typedef void (*mk_watchdog_callback_t)(const char* task_name);

mk_status_t mk_diagnostics_init(void);
void mk_diagnostics_tick(void);

uint32_t mk_task_get_stack_high_watermark(mk_task_handle_t task);
uint64_t mk_task_get_runtime(mk_task_handle_t task);
uint32_t mk_kernel_get_cpu_load(void);
void mk_kernel_print_stats(void);

void mk_watchdog_register(const char* name, uint32_t timeout_ms, mk_watchdog_callback_t cb);
mk_watchdog_token_t mk_watchdog_register_task(mk_task_handle_t task, uint32_t timeout_ms, mk_watchdog_callback_t cb);
void mk_watchdog_feed(const char* name);
void mk_watchdog_feed_token(mk_watchdog_token_t token);
void mk_watchdog_feed_self(void);
bool mk_diagnostics_healthy(void);
const char* mk_diagnostics_health_text(void);
uint32_t mk_watchdog_age_ms(const char* name);
void mk_diagnostics_copy_health(char* dst, size_t dst_len);

#ifdef __cplusplus
}
#endif

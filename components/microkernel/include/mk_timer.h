#pragma once
#include "mk_types.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mk_timer mk_timer_t;
typedef void (*mk_timer_callback_t)(void* arg);

mk_timer_t* mk_timer_create(const char* name, uint32_t period_ms, bool auto_reload, mk_timer_callback_t cb, void* arg);
mk_status_t mk_timer_delete(mk_timer_t* timer);
mk_status_t mk_timer_start(mk_timer_t* timer, uint32_t timeout_ms);
mk_status_t mk_timer_stop(mk_timer_t* timer, uint32_t timeout_ms);
mk_status_t mk_timer_reset(mk_timer_t* timer);
bool mk_timer_is_active(mk_timer_t* timer);
uint64_t mk_clock_get_ms(void);
#ifdef __cplusplus
}
#endif

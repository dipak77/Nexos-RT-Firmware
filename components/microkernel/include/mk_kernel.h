#pragma once
#include "mk_config.h"
#include "mk_types.h"
#include "mk_task.h"
#include "mk_queue.h"
#include "mk_mutex.h"
#include "mk_semaphore.h"
#include "mk_event.h"
#include "mk_timer.h"
#include "mk_memory.h"
#include "mk_diagnostics.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t tick_hz;
    bool use_preemption;
    bool single_core;
    const char* version;
} mk_config_t;

mk_status_t mk_init(const mk_config_t* config);
mk_status_t mk_start(void);
void mk_yield(void);
void mk_sleep_ms(uint32_t ms);
uint64_t mk_time_ms(void);
uint64_t mk_time_us(void);
bool mk_is_initialized(void);
bool mk_is_running(void);
mk_kernel_stats_t mk_kernel_get_stats(void);
int mk_current_core(void);
void mk_wait_start(void);
uint32_t mk_map_port_priority(uint8_t mk_prio);

// Umbrella include for app - this is the ONLY header app should include
// App code: #include "mk/mk.h"

#ifdef __cplusplus
}
#endif

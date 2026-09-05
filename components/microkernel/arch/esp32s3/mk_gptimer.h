#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the 1kHz kernel tick (single source, see mk_scheduler tick
 * driver). Idempotent alias — safe to call alongside mk_scheduler_start_tick().
 * NOTE: backed by esp_timer, not a dedicated HW GPTimer alarm; named for the
 * future HW-timer backend which will keep this same API.
 * @return true on success, false on failure.
 */
bool mk_gptimer_init(void);

/**
 * @brief Start the 1kHz tick (delegates to scheduler driver; idempotent).
 */
void mk_gptimer_start(void);

/**
 * @brief Stop the 1kHz tick.
 */
void mk_gptimer_stop(void);

/**
 * @brief Get high-resolution monotonic microsecond timestamp from hardware counter.
 */
uint64_t mk_gptimer_get_time_us(void);

#ifdef __cplusplus
}
#endif

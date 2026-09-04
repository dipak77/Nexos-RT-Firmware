#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the hardware GPTimer on ESP32-S3 for Nexos-RT native scheduling.
 * Configured for 1kHz (1000us) tick rate.
 * @return true on success, false on failure.
 */
bool mk_gptimer_init(void);

/**
 * @brief Start the 1kHz hardware tick timer.
 */
void mk_gptimer_start(void);

/**
 * @brief Stop the hardware tick timer.
 */
void mk_gptimer_stop(void);

/**
 * @brief Get high-resolution monotonic microsecond timestamp from hardware counter.
 */
uint64_t mk_gptimer_get_time_us(void);

#ifdef __cplusplus
}
#endif

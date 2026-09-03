#pragma once
#include "mk_types.h"
#ifdef __cplusplus
extern "C" {
#endif

// Health subsystem initialization
void mk_health_init(void);

// Record fault event (deducts health score and records fault code)
void mk_health_record_fault(const char* fault_code, uint8_t penalty);

// Periodic health tick (handles score recovery when healthy)
void mk_health_tick(void);

// Get a snapshot of the current health state
mk_health_snapshot_t mk_health_get_snapshot(void);

// Set/Get operating mode
mk_health_mode_t mk_health_get_mode(void);
void mk_health_set_mode(mk_health_mode_t mode);

// Check if system is currently degraded or safe mode
bool mk_health_is_healthy(void);

#ifdef __cplusplus
}
#endif

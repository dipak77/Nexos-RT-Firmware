#pragma once
// Native Nexos-RT port definitions — NO FreeRTOS.
// Provides spinlock, list, and native handle stubs for ESP32-S3.

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// ESP32-S3 interrupt level for critical sections (maps to XTENSA PS.INTLEVEL)
#include "esp_cpu.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"

// Compat shims for native build without FreeRTOS
// If FreeRTOS already included (native port .c includes it privately), use its definitions
#ifndef INC_FREERTOS_H
  #ifndef portMUX_TYPE
  typedef volatile uint32_t portMUX_TYPE;
  #define portMUX_INITIALIZER_UNLOCKED 0
  #endif
  #ifndef portMAX_DELAY
  #define portMAX_DELAY 0xFFFFFFFFu
  #endif
  #ifndef configMAX_PRIORITIES
  #define configMAX_PRIORITIES 25
  #endif
#endif

// Lightweight spinlock for SMP (uses esp_cpu compare-and-set)
typedef volatile uint32_t mk_native_spinlock_t;
#define MK_NATIVE_SPINLOCK_INIT 0

static inline uint32_t mk_native_irq_save(void) {
    uint32_t ps;
    // Must not clobber a0 (return address on windowed Xtensa).
    __asm__ volatile ("rsil %0, 15" : "=a"(ps) :: "memory");
    return ps;
}
static inline void mk_native_irq_restore(uint32_t ps) {
    __asm__ volatile ("wsr %0, PS; rsync" :: "a"(ps) : "memory");
}
static inline void mk_native_spin_lock(mk_native_spinlock_t* l) {
    (void)l;
    (void)mk_native_irq_save();
}
static inline void mk_native_spin_unlock(mk_native_spinlock_t* l) {
    (void)l;
    mk_native_irq_restore(0);
}

// Native handle types (opaque)
typedef struct mk_native_task mk_native_task_t;
typedef struct mk_native_mutex mk_native_mutex_t;
typedef struct mk_native_queue mk_native_queue_t;
typedef struct mk_native_event mk_native_event_t;
typedef struct mk_native_sem mk_native_sem_t;

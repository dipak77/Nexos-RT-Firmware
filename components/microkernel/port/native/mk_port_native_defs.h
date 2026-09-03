#pragma once
// Native Nexos-RT port definitions — NO FreeRTOS exposed to core.
// Provides spinlock, nested IRQ-safe critical sections, and native handle stubs for ESP32-S3.

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
#ifndef INC_FREERTOS_H
  #ifndef portMUX_TYPE
  typedef struct {
      volatile uint32_t owner;
      volatile uint32_t count;
      volatile uint32_t saved_ps;
  } portMUX_TYPE;
  #define portMUX_INITIALIZER_UNLOCKED { .owner = 0xB33FFFFF, .count = 0, .saved_ps = 0 }
  #endif
  #ifndef portMUX_INITIALIZE
  #define portMUX_INITIALIZE(mux) do { if(mux) { memset((void*)(mux), 0, sizeof(*(mux))); } } while(0)
  #endif
  #ifndef portMAX_DELAY
  #define portMAX_DELAY 0xFFFFFFFFu
  #endif
  #ifndef configMAX_PRIORITIES
  #define configMAX_PRIORITIES 25
  #endif
#endif

// Nested IRQ save/restore for Xtensa LX7
static inline uint32_t mk_native_irq_save(void) {
    uint32_t ps;
    // Must not clobber a0 (return address on windowed Xtensa).
    __asm__ volatile ("rsil %0, 15" : "=a"(ps) :: "memory");
    return ps;
}

static inline void mk_native_irq_restore(uint32_t ps) {
    __asm__ volatile ("wsr %0, PS; rsync" :: "a"(ps) : "memory");
}

// Lightweight spinlock for SMP with nesting counter
typedef struct {
    volatile uint32_t lock;
    volatile uint32_t nesting;
    volatile uint32_t saved_ps;
} mk_native_spinlock_t;

#define MK_NATIVE_SPINLOCK_INIT { .lock = 0, .nesting = 0, .saved_ps = 0 }

static inline void mk_native_spin_lock(mk_native_spinlock_t* l) {
    uint32_t ps = mk_native_irq_save();
    if (l) {
        if (l->nesting == 0) {
            l->saved_ps = ps;
            l->lock = 1;
        }
        l->nesting++;
    }
}

static inline void mk_native_spin_unlock(mk_native_spinlock_t* l) {
    if (l && l->nesting > 0) {
        l->nesting--;
        if (l->nesting == 0) {
            l->lock = 0;
            uint32_t ps = l->saved_ps;
            mk_native_irq_restore(ps);
            return;
        }
    }
}

// Native handle types (opaque)
typedef struct mk_native_task mk_native_task_t;
typedef struct mk_native_mutex mk_native_mutex_t;
typedef struct mk_native_queue mk_native_queue_t;
typedef struct mk_native_event mk_native_event_t;
typedef struct mk_native_sem mk_native_sem_t;

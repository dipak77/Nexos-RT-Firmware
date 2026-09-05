#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "mk_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MK_MAX_ENCLAVES 4

/* Capability Bitmasks */
#define MK_CAP_NONE        0x00000000u
#define MK_CAP_GFX         (1u << 0)
#define MK_CAP_NET         (1u << 1)
#define MK_CAP_STORAGE     (1u << 2)
#define MK_CAP_GPIO        (1u << 3)
#define MK_CAP_AUDIO       (1u << 4)
#define MK_CAP_SYSTEM      (1u << 5)
#define MK_CAP_ALL         0xFFFFFFFFu

typedef enum {
    MK_ENCLAVE_FREE = 0,
    MK_ENCLAVE_RESERVED,
    MK_ENCLAVE_LIVE,
    MK_ENCLAVE_FAILED,
    MK_ENCLAVE_RECLAIMING
} mk_enclave_state_t;

typedef enum {
    MK_ENCLAVE_TYPE_RT = 0,
    MK_ENCLAVE_TYPE_INFER,
    MK_ENCLAVE_TYPE_SYSTEM
} mk_enclave_type_t;

typedef struct mk_enclave_desc {
    uint32_t id;
    char name[16];
    
    /* Memory bounds (validated at SVC boundary) */
    uintptr_t base;
    uintptr_t limit;
    uintptr_t stack_base;
    size_t stack_size;
    uint32_t asid; /* Slot / Region ID */

    /* Timing contract (Claim 2) */
    uint32_t deadline_us;
    uint32_t budget_us;
    uint32_t remaining_budget_us;
    uint8_t priority;
    uint8_t type;

    /* Privileges & Hardware Isolation (Claims 3, 7) */
    uint8_t watchdog_id;
    uint32_t syscall_mask;
    uint8_t state;

    /* Resource Reclamation & Safety (Claims 7, 8) */
    uint32_t held_mutex_mask;
    bool dma_active;
    mk_task_handle_t task_handle;
} mk_enclave_desc_t;

/**
 * @brief Initialize the microkernel enclave manager.
 */
mk_status_t mk_enclave_init(void);

/**
 * @brief Create a new capability-isolated enclave.
 */
mk_enclave_desc_t* mk_enclave_create(
    const char* name,
    uint8_t priority,
    uint8_t type,
    uintptr_t base,
    uintptr_t limit,
    size_t stack_size,
    uint32_t syscall_mask,
    uint32_t budget_us,
    uint32_t deadline_us
);

/**
 * @brief Launch an enclave with its entry point.
 */
mk_status_t mk_enclave_start(mk_enclave_desc_t* desc, mk_task_entry_t entry, void* arg);

/**
 * @brief Retrieve descriptor of currently executing enclave.
 */
mk_enclave_desc_t* mk_enclave_get_current(void);

/**
 * @brief Retrieve enclave descriptor by slot ID.
 */
mk_enclave_desc_t* mk_enclave_get_by_id(uint32_t id);

/**
 * @brief Register a heap/data window for SVC pointer checks.
 * Until called (base==0), heap-pointer SVC buffers fail closed while stack
 * buffers are still validated. The window must be wholly owned by the caller.
 */
mk_status_t mk_enclave_set_heap_window(mk_enclave_desc_t* desc, uintptr_t base, uintptr_t limit);

/**
 * @brief Trigger an enclave fault trap: mark FAILED, log typed fault, halt task.
 * Never deletes: the enclave stays FAILED and visible until the operator (or
 * policy) calls mk_enclave_reclaim/reset, which frees the slot. Safe to call
 * on the running task itself (no self-suspend deadlock, no live-stack memset).
 */
mk_status_t mk_enclave_trap(mk_enclave_desc_t* desc, uint8_t fault_type, uint16_t fault_code, const char* reason);

/**
 * @brief Safely reclaim all resources of a failed enclave (mutexes, DMA, stack zeroing).
 */
mk_status_t mk_enclave_reclaim(mk_enclave_desc_t* desc);

/**
 * @brief Reset and reinitialize an enclave by ID.
 */
mk_status_t mk_enclave_reset(uint32_t id);

/**
 * @brief Get total number of live enclaves.
 */
uint32_t mk_enclave_get_live_count(void);

/**
 * @brief 10Hz budget sampler (called from scheduler tick tail, outside locks).
 * Matches each budgeted LIVE enclave against its REAL FreeRTOS run-time
 * counter via the port layer. On exhaust: FAILED + fault log + real suspend.
 * Dormant when run-time stats are disabled (all deltas read 0) or when no
 * enclave carries budget_us > 0. Never allocates; never blocks.
 */
void mk_enclave_sample_budgets(void);

/**
 * @brief Print status of all enclaves to console.
 */
void mk_enclave_dump_status(void);

#ifdef __cplusplus
}
#endif

#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MK_FAULT_RING_SIZE 128

typedef enum {
    MK_FAULT_NONE = 0,
    MK_FAULT_CAPABILITY_VIOLATION = 1,
    MK_FAULT_OOB_MEMORY_ACCESS = 2,
    MK_FAULT_BUDGET_EXHAUSTED = 3,
    MK_FAULT_WATCHDOG_TIMEOUT = 4,
    MK_FAULT_STACK_OVERFLOW = 5,
    MK_FAULT_ILLEGAL_INSTRUCTION = 6,
    MK_FAULT_DEADLOCK = 7,
    MK_FAULT_DMA_ERROR = 8,
} mk_fault_type_t;

typedef struct {
    uint32_t timestamp_ms;
    uint8_t  enclave_id;
    uint8_t  fault_type;
    uint16_t fault_code;
    uint32_t pc;
    uint32_t sp;
    uint32_t extra_info;
} mk_fault_record_t;

/**
 * @brief Initialize static 128-entry fault ringbuffer in internal SRAM.
 */
void mk_fault_init(void);

/**
 * @brief Push a new fault record into the circular ring. Zero heap allocations.
 */
void mk_fault_log(uint8_t enclave_id, mk_fault_type_t type, uint16_t code, uint32_t pc, uint32_t sp, uint32_t extra);

/**
 * @brief Get total number of faults recorded since boot.
 */
uint32_t mk_fault_get_total_count(void);

/**
 * @brief Read record at specified ring index.
 */
bool mk_fault_get_record(uint32_t index, mk_fault_record_t* out_record);

/**
 * @brief Clear all fault records from the ring.
 */
void mk_fault_clear(void);

/**
 * @brief Print all logged faults formatted as a diagnostic table.
 */
void mk_fault_dump(void);

#ifdef __cplusplus
}
#endif

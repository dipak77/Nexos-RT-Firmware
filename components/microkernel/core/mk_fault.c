#include "mk_fault.h"
#include "mk_port.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>
#include <stdio.h>

static const char* TAG = "MK_FAULT";

static mk_fault_record_t s_fault_ring[MK_FAULT_RING_SIZE];
static uint32_t s_head = 0;
static uint32_t s_total_count = 0;

void mk_fault_init(void) {
    mk_port_enter_critical();
    memset(s_fault_ring, 0, sizeof(s_fault_ring));
    s_head = 0;
    s_total_count = 0;
    mk_port_exit_critical();
    ESP_LOGI(TAG, "Zero-allocation 128-entry SRAM fault ring initialized");
}

void mk_fault_log(uint8_t enclave_id, mk_fault_type_t type, uint16_t code, uint32_t pc, uint32_t sp, uint32_t extra) {
    mk_port_enter_critical();
    uint32_t idx = s_head % MK_FAULT_RING_SIZE;
    s_fault_ring[idx].timestamp_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
    s_fault_ring[idx].enclave_id = enclave_id;
    s_fault_ring[idx].fault_type = (uint8_t)type;
    s_fault_ring[idx].fault_code = code;
    s_fault_ring[idx].pc = pc;
    s_fault_ring[idx].sp = sp;
    s_fault_ring[idx].extra_info = extra;

    s_head++;
    s_total_count++;
    mk_port_exit_critical();
}

uint32_t mk_fault_get_total_count(void) {
    return s_total_count;
}

bool mk_fault_get_record(uint32_t index, mk_fault_record_t* out_record) {
    if (!out_record) return false;
    mk_port_enter_critical();
    uint32_t count = s_total_count > MK_FAULT_RING_SIZE ? MK_FAULT_RING_SIZE : s_total_count;
    if (index >= count) {
        mk_port_exit_critical();
        return false;
    }

    uint32_t start = (s_total_count > MK_FAULT_RING_SIZE) ? (s_head - MK_FAULT_RING_SIZE) : 0;
    uint32_t ring_idx = (start + index) % MK_FAULT_RING_SIZE;
    *out_record = s_fault_ring[ring_idx];
    mk_port_exit_critical();
    return true;
}

void mk_fault_clear(void) {
    mk_port_enter_critical();
    memset(s_fault_ring, 0, sizeof(s_fault_ring));
    s_head = 0;
    s_total_count = 0;
    mk_port_exit_critical();
    ESP_LOGI(TAG, "Fault ring cleared");
}

static const char* fault_type_str(uint8_t t) {
    switch (t) {
        case MK_FAULT_CAPABILITY_VIOLATION: return "CAPABILITY_VIOLATION";
        case MK_FAULT_OOB_MEMORY_ACCESS:   return "OOB_ACCESS";
        case MK_FAULT_BUDGET_EXHAUSTED:     return "BUDGET_EXHAUSTED";
        case MK_FAULT_WATCHDOG_TIMEOUT:     return "WATCHDOG_TIMEOUT";
        case MK_FAULT_STACK_OVERFLOW:       return "STACK_OVERFLOW";
        case MK_FAULT_ILLEGAL_INSTRUCTION:  return "ILLEGAL_INSTRUCTION";
        case MK_FAULT_DEADLOCK:             return "DEADLOCK";
        case MK_FAULT_DMA_ERROR:            return "DMA_ERROR";
        default:                            return "UNKNOWN";
    }
}

void mk_fault_dump(void) {
    mk_port_enter_critical();
    uint32_t count = s_total_count > MK_FAULT_RING_SIZE ? MK_FAULT_RING_SIZE : s_total_count;
    uint32_t total = s_total_count;
    mk_port_exit_critical();

    printf("\n=== Nexos-RT V2 Structured Fault Ring (Total: %lu, Retained: %lu) ===\n",
           (unsigned long)total, (unsigned long)count);
    printf("IDX | TIME(ms) | ENCLAVE | TYPE                | CODE | PC         | SP         | EXTRA\n");
    printf("----+----------+---------+---------------------+------+------------+------------+-----------\n");

    mk_fault_record_t rec;
    for (uint32_t i = 0; i < count; i++) {
        if (mk_fault_get_record(i, &rec)) {
            printf("%3lu | %8lu | %7u | %-19s | %04X | 0x%08lX | 0x%08lX | 0x%08lX\n",
                   (unsigned long)i,
                   (unsigned long)rec.timestamp_ms,
                   rec.enclave_id,
                   fault_type_str(rec.fault_type),
                   rec.fault_code,
                   (unsigned long)rec.pc,
                   (unsigned long)rec.sp,
                   (unsigned long)rec.extra_info);
        }
    }
    printf("======================================================================================\n\n");
}

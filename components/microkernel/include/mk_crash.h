#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MK_CRASH_MAGIC 0x4E455843 // "NEXC"

typedef struct {
    uint32_t magic;
    char cause[16];
    char task_name[32];
    uint32_t fault_pc;
    uint32_t uptime_s;
    uint32_t free_heap_at_fault;
    uint32_t reboot_count;
    char last_trace[64];
} mk_crash_record_t;

void mk_crash_init(void);
void mk_crash_save(const char* cause, const char* task_name, uint32_t fault_pc, const char* trace);
bool mk_crash_has_record(void);
bool mk_crash_get_record(mk_crash_record_t* record);
void mk_crash_clear(void);

#ifdef __cplusplus
}
#endif

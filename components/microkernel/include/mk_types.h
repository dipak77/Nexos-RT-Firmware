#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "mk_config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MK_OK = 0,
    MK_ERR_INVALID = 1000,
    MK_ERR_NO_MEMORY = 1001,
    MK_ERR_TIMEOUT = 1002,
    MK_ERR_BUSY = 1003,
    MK_ERR_NOT_INITIALIZED = 1004,
    MK_ERR_ALREADY_EXISTS = 1005,
    MK_ERR_NOT_FOUND = 1006,
    MK_ERR_OVERFLOW = 1007,
    MK_ERR_UNDERFLOW = 1008,
    MK_ERR_ISR = 1009,
    MK_ERR_PRIORITY = 1010,
    MK_ERR_BAD_STATE = 1011,
} mk_status_t;

typedef enum {
    MK_TASK_READY = 0,
    MK_TASK_RUNNING,
    MK_TASK_BLOCKED,
    MK_TASK_SLEEPING,
    MK_TASK_SUSPENDED,
    MK_TASK_DELETED
} mk_task_state_t;

typedef struct mk_task {
    uint32_t id;
    uint8_t priority;
    mk_task_state_t state;
    size_t stack_size;
    void* stack_base;
    void* stack_pointer;
    const char* name;
    struct mk_task* next;
} mk_task_t;
typedef void (*mk_task_entry_t)(void* arg);
typedef mk_task_t* mk_task_handle_t;

typedef struct {
    const char* name;
    uint8_t priority;
    size_t stack_size;
    void* stack_base; // if NULL, kernel allocates
    int core_affinity; // -1 any, 0/1 pinned - for ESP32-S3 platform shim
    bool static_alloc;
} mk_task_config_t;

typedef struct {
    uint32_t total_tasks;
    uint32_t ready_tasks;
    uint64_t uptime_ms;
    uint64_t context_switches;
    uint32_t cpu_load_percent; // 0-100
    size_t free_heap;
    size_t min_free_heap;
} mk_kernel_stats_t;

typedef struct {
    uint32_t id;
    char name[32];
    mk_task_state_t state;
    uint8_t priority;
    uint8_t base_priority; // for PI
    uint32_t stack_size;
    uint32_t stack_high_watermark;
    uint64_t runtime_ticks;
    uint64_t wake_time_ms;
} mk_task_info_t;

const char* mk_status_to_string(mk_status_t s);
const char* mk_task_state_to_string(mk_task_state_t s);

#ifdef __cplusplus
}
#endif

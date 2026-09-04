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
    MK_ERR_DEADLOCK = 1012,
    MK_ERR_INTERRUPTED = 1013,
    MK_ERR_DEADLOCK_OWNER_DEAD = 1014,
    MK_ERR_PEER_TERMINATED = 1015,
    MK_ERR_CAPABILITY = 1016,
    MK_ERR_SVC_FAULT = 1017,
} mk_status_t;

typedef enum {
    MK_TASK_READY = 0,
    MK_TASK_RUNNING,
    MK_TASK_BLOCKED,
    MK_TASK_SLEEPING,
    MK_TASK_SUSPENDED,
    MK_TASK_DELETED,
    MK_TASK_ZOMBIE
} mk_task_state_t;

typedef struct mk_task {
    uint32_t id;
    uint8_t priority;
    uint8_t base_priority; // for Priority Inheritance
    mk_task_state_t state;
    size_t stack_size;
    void* stack_base;
    void* stack_pointer;
    const char* name;
    uint32_t time_slice;
    uint64_t wake_tick;
    uint64_t runtime_ticks;
    int core_affinity;
    /* V2 Architecture: Enclave & Timing Contracts */
    void* enclave_desc;
    uint32_t deadline_us;
    uint32_t budget_us;
    uint32_t remaining_budget_us;
    struct mk_task* next;
    struct mk_task* prev;
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

typedef enum {
    MK_HEALTH_MODE_NORMAL = 0,
    MK_HEALTH_MODE_DEGRADED_UI,
    MK_HEALTH_MODE_DEGRADED_NET,
    MK_HEALTH_MODE_SAFE_MODE,
} mk_health_mode_t;

typedef struct {
    uint8_t device_score; // 0 - 100
    uint8_t mem_score;
    uint8_t crash_score;
    uint8_t net_score;
    char fault_code[16];
    uint32_t uptime_s;
    uint32_t reboot_count;
    uint32_t stall_count;
    mk_health_mode_t mode;
} mk_health_snapshot_t;

const char* mk_status_to_string(mk_status_t s);
const char* mk_task_state_to_string(mk_task_state_t s);
const char* mk_health_mode_to_string(mk_health_mode_t m);

#ifdef __cplusplus
}
#endif

#include "mk_enclave.h"
#include "mk_fault.h"
#include "mk_task.h"
#include "mk_scheduler.h"
#include "mk_port.h"
#include "mk_watchpoint.h"
#include "mk_mutex.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>
#include <stdio.h>

static const char* TAG = "MK_ENCLAVE";

static mk_enclave_desc_t s_enclaves[MK_MAX_ENCLAVES];
static bool s_enclave_mgr_initialized = false;

// Previous run-time counter per slot for sampler delta accounting. Static,
// no alloc. Reset whenever a slot returns to FREE so reuse starts clean.
static uint32_t s_prev_runtime[MK_MAX_ENCLAVES] = {0};
static bool s_prev_valid[MK_MAX_ENCLAVES] = {false};
static uint32_t s_prev_task_id[MK_MAX_ENCLAVES] = {0};

mk_status_t mk_enclave_init(void) {
    mk_port_enter_critical();
    memset(s_enclaves, 0, sizeof(s_enclaves));
    for (uint32_t i = 0; i < MK_MAX_ENCLAVES; i++) {
        s_enclaves[i].id = i;
        s_enclaves[i].asid = i;
        s_enclaves[i].state = MK_ENCLAVE_FREE;
    }
    s_enclave_mgr_initialized = true;
    mk_port_exit_critical();

    mk_fault_init();
    ESP_LOGI(TAG, "Nexos-RT V2 Enclave Manager initialized (Max Enclaves: %d)", MK_MAX_ENCLAVES);
    return MK_OK;
}

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
) {
    if (!s_enclave_mgr_initialized) {
        mk_enclave_init();
    }

    mk_port_enter_critical();
    mk_enclave_desc_t* slot = NULL;
    for (uint32_t i = 0; i < MK_MAX_ENCLAVES; i++) {
        if (s_enclaves[i].state == MK_ENCLAVE_FREE) {
            slot = &s_enclaves[i];
            break;
        }
    }

    if (!slot) {
        mk_port_exit_critical();
        ESP_LOGE(TAG, "No free enclave slots available for %s", name ? name : "enclave");
        return NULL;
    }

    slot->state = MK_ENCLAVE_RESERVED;
    strncpy(slot->name, name ? name : "enclave", sizeof(slot->name) - 1);
    slot->name[sizeof(slot->name) - 1] = '\0';
    slot->priority = priority;
    slot->type = type;
    slot->base = base;
    slot->limit = limit;
    slot->stack_size = stack_size;
    slot->stack_base = 0; // Configured at start
    slot->syscall_mask = syscall_mask;
    slot->budget_us = budget_us;
    slot->remaining_budget_us = budget_us;
    slot->deadline_us = deadline_us;
    slot->watchdog_id = (uint8_t)slot->id;
    slot->held_mutex_mask = 0;
    slot->dma_active = false;
    slot->task_handle = NULL;

    mk_port_exit_critical();
    ESP_LOGI(TAG, "Enclave created: [%s] id=%lu prio=%u type=%u caps=0x%08lX budget=%luus",
             slot->name, (unsigned long)slot->id, slot->priority, slot->type,
             (unsigned long)slot->syscall_mask, (unsigned long)slot->budget_us);
    return slot;
}

mk_status_t mk_enclave_start(mk_enclave_desc_t* desc, mk_task_entry_t entry, void* arg) {
    if (!desc || desc->state != MK_ENCLAVE_RESERVED) {
        return MK_ERR_INVALID;
    }

    mk_task_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.name = desc->name;
    cfg.priority = desc->priority;
    cfg.stack_size = desc->stack_size;
    cfg.core_affinity = 1; // Application domain Core 1
    cfg.static_alloc = false;
    cfg.defer_start = true; // A6: Latch execution until enclave attributes are fully published

    mk_task_handle_t t = mk_task_create_ext(&cfg, entry, arg);
    if (!t) {
        desc->state = MK_ENCLAVE_FAILED;
        return MK_ERR_NO_MEMORY;
    }

    mk_port_enter_critical();
    desc->task_handle = t;
    // Real stack window: t->stack_base is always NULL for port-created tasks
    // (mk_task_create_ext never copies config->stack_base), so query the
    // backend. Falls back to 0 = "stack window unknown", in which case SVC
    // pointer checks and the watchpoint guard stay dormant for this enclave.
    desc->stack_base = 0;
    mk_port_exit_critical();
    uintptr_t real_base = 0;
    void* port_handle = mk_task_get_port_handle(t);
    if (port_handle && mk_port_task_stack_base(port_handle, &real_base) && real_base) {
        mk_port_enter_critical();
        // Re-validate: task may have been deleted concurrently.
        if (desc->task_handle == t) desc->stack_base = real_base;
        mk_port_exit_critical();
    }
    mk_port_enter_critical();
    desc->state = MK_ENCLAVE_LIVE;

    // Attach enclave descriptor to microkernel task
    t->enclave_desc = (void*)desc;
    t->budget_us = desc->budget_us;
    t->remaining_budget_us = desc->budget_us;
    t->deadline_us = desc->deadline_us;
    mk_port_exit_critical();

    // A9: Capture baseline runtime ticks at creation so initial execution is not dropped
    uint32_t init_runtime = 0;
    if (port_handle && mk_port_task_runtime(port_handle, &init_runtime)) {
        s_prev_runtime[desc->id] = init_runtime;
        s_prev_valid[desc->id] = true;
    } else {
        s_prev_runtime[desc->id] = 0;
        s_prev_valid[desc->id] = false;
    }
    s_prev_task_id[desc->id] = t->id;

    // Arm hardware debug watchpoint on stack bottom to trap overflows
    if (desc->stack_base && desc->id < 2) {
        mk_watchpoint_arm_stack_guard((int)desc->id, (void*)desc->stack_base, 32);
    }

    // A6: Release per-task creation latch now that enclave, bounds, and budget are published
    mk_task_release_latch(t);

    ESP_LOGI(TAG, "Enclave [%s] is now LIVE (task=%p stack=%p-%p)",
             desc->name, t, (void*)desc->stack_base, (void*)(desc->stack_base + desc->stack_size));
    return MK_OK;
}

mk_enclave_desc_t* mk_enclave_get_current(void) {
    mk_task_t* cur = mk_scheduler_current_task();
    if (cur && cur->enclave_desc) {
        return (mk_enclave_desc_t*)cur->enclave_desc;
    }
    return NULL;
}

mk_enclave_desc_t* mk_enclave_get_by_id(uint32_t id) {
    if (id >= MK_MAX_ENCLAVES) return NULL;
    return &s_enclaves[id];
}

mk_status_t mk_enclave_set_heap_window(mk_enclave_desc_t* desc, uintptr_t base, uintptr_t limit) {
    if (!desc || limit <= base) return MK_ERR_INVALID;
    mk_port_enter_critical();
    desc->base = base;
    desc->limit = limit;
    mk_port_exit_critical();
    return MK_OK;
}

mk_status_t mk_enclave_trap(mk_enclave_desc_t* desc, uint8_t fault_type, uint16_t fault_code, const char* reason) {
    if (!desc) return MK_ERR_INVALID;

    // Snapshot identity first: a self-trap must not touch desc after halting.
    char name[16];
    uint32_t enc_id;
    mk_task_handle_t victim;
    bool victim_is_self;
    mk_port_enter_critical();
    desc->state = MK_ENCLAVE_FAILED;
    strncpy(name, desc->name, sizeof(name) - 1);
    name[sizeof(name) - 1] = '\0';
    enc_id = desc->id;
    victim = desc->task_handle;
    victim_is_self = (victim != NULL && victim == mk_task_self());
    mk_port_exit_critical();

    ESP_LOGE(TAG, "TRAP FIRED in Enclave [%s] (id=%lu): type=%u code=0x%04X, reason=%s",
             name, (unsigned long)enc_id, fault_type, fault_code, reason ? reason : "unknown");
    mk_fault_log((uint8_t)enc_id, (mk_fault_type_t)fault_type, fault_code, 0, 0, 0);

    if (!victim) return MK_OK; // nothing executing: FAILED + logged is enough

    if (victim_is_self) {
        // Self-trap: suspending self never returns, so reclaim mutexes first
        // (marks OWNER_DEAD, never touches live locks), disarm the watchpoint,
        // clear references, then self-delete.
        mk_mutex_reclaim_for_task(victim);
        mk_port_enter_critical();
        desc->held_mutex_mask = 0;
        desc->task_handle = NULL;
        desc->stack_base = 0;
        mk_port_exit_critical();
        if (desc->id < 2) mk_watchpoint_disarm((int)desc->id);
        mk_task_delete(victim); // self-delete: never returns
        return MK_OK; // unreachable; keeps callers honest if port ever returns
    }

    // Other-task trap: suspend FIRST so the victim can never resume between
    // sanitize and delete, then sanitize, then delete.
    mk_task_suspend(victim);
    mk_mutex_reclaim_for_task(victim);
    mk_port_enter_critical();
    desc->held_mutex_mask = 0;
    mk_port_exit_critical();
    if (desc->id < 2) mk_watchpoint_disarm((int)desc->id);
    if (desc->stack_base && desc->stack_size > 0) {
        memset((void*)desc->stack_base, 0, desc->stack_size);
    }
    mk_task_delete(victim);
    mk_port_enter_critical();
    desc->task_handle = NULL;
    desc->stack_base = 0;
    // State stays FAILED (visible for audit) until reclaim frees the slot.
    mk_port_exit_critical();
    return MK_OK;
}

mk_status_t mk_enclave_reclaim(mk_enclave_desc_t* desc) {
    if (!desc) return MK_ERR_INVALID;

    mk_port_enter_critical();
    if (desc->state == MK_ENCLAVE_FREE) {
        mk_port_exit_critical();
        return MK_OK;
    }
    if (desc->state != MK_ENCLAVE_FAILED && desc->state != MK_ENCLAVE_LIVE &&
        desc->state != MK_ENCLAVE_RESERVED) {
        mk_port_exit_critical();
        return MK_ERR_BAD_STATE;
    }
    desc->state = MK_ENCLAVE_RECLAIMING;
    mk_task_handle_t victim = desc->task_handle;
    bool victim_is_self = (victim != NULL && victim == mk_task_self());
    mk_port_exit_critical();

    ESP_LOGW(TAG, "Reclaiming resources for Enclave [%s]...", desc->name);

    // Driver-level DMA abort is a HAL future hook; today we only drop the flag
    // so no caller mistakes the log line for a completed abort.
    if (desc->dma_active) {
        ESP_LOGW(TAG, "Enclave [%s] had DMA marked active; driver abort is a HAL TODO, flag cleared", desc->name);
        desc->dma_active = false;
    }

    if (victim) {
        if (victim_is_self) {
            // A4: Self-reclaim must transition to FREE before deleting self,
            // otherwise descriptor is permanently stranded in RECLAIMING.
            mk_mutex_reclaim_for_task(victim);
            if (desc->id < 2) mk_watchpoint_disarm((int)desc->id);
            mk_port_enter_critical();
            desc->held_mutex_mask = 0;
            desc->task_handle = NULL;
            desc->stack_base = 0;
            desc->state = MK_ENCLAVE_FREE;
            s_prev_valid[desc->id] = false;
            mk_port_exit_critical();
            mk_task_delete(victim); // never returns
            return MK_OK;
        }
        mk_task_state_t st = mk_task_get_state(victim);
        if (st != MK_TASK_DELETED && st != MK_TASK_ZOMBIE) {
            mk_task_suspend(victim);
            mk_mutex_reclaim_for_task(victim);
            if (desc->id < 2) mk_watchpoint_disarm((int)desc->id);
            if (desc->stack_base && desc->stack_size > 0) {
                memset((void*)desc->stack_base, 0, desc->stack_size);
            }
            mk_task_delete(victim);
        } else {
            if (desc->id < 2) mk_watchpoint_disarm((int)desc->id);
        }
    } else {
        if (desc->id < 2) mk_watchpoint_disarm((int)desc->id);
    }
    desc->held_mutex_mask = 0;

    mk_port_enter_critical();
    desc->task_handle = NULL;
    desc->stack_base = 0;
    desc->state = MK_ENCLAVE_FREE;
    s_prev_valid[desc->id] = false; // budget sampler restarts clean on reuse
    mk_port_exit_critical();

    ESP_LOGI(TAG, "Enclave [%s] successfully reclaimed and reset to FREE", desc->name);
    return MK_OK;
}

mk_status_t mk_enclave_reset(uint32_t id) {
    if (id >= MK_MAX_ENCLAVES) return MK_ERR_INVALID;
    return mk_enclave_reclaim(&s_enclaves[id]);
}

uint32_t mk_enclave_get_live_count(void) {
    uint32_t count = 0;
    mk_port_enter_critical();
    for (uint32_t i = 0; i < MK_MAX_ENCLAVES; i++) {
        if (s_enclaves[i].state == MK_ENCLAVE_LIVE) count++;
    }
    mk_port_exit_critical();
    return count;
}

void mk_enclave_sample_budgets(void) {
    // Snapshot budgeted LIVE enclaves + their backend handles under one lock.
    struct { mk_enclave_desc_t* desc; mk_task_handle_t task; void* port; } items[MK_MAX_ENCLAVES];
    int n = 0;
    mk_port_enter_critical();
    for (uint32_t i = 0; i < MK_MAX_ENCLAVES; i++) {
        if (s_enclaves[i].state == MK_ENCLAVE_LIVE &&
            s_enclaves[i].budget_us > 0 && s_enclaves[i].task_handle) {
            items[n].desc = &s_enclaves[i];
            items[n].task = s_enclaves[i].task_handle;
            items[n].port = mk_task_get_port_handle(s_enclaves[i].task_handle);
            n++;
        }
    }
    mk_port_exit_critical();
    if (n == 0) return;

    for (int k = 0; k < n; k++) {
        uint32_t total = 0;
        if (!items[k].port || !mk_port_task_runtime(items[k].port, &total)) continue;
        uint32_t id = items[k].desc->id;
        if (id >= MK_MAX_ENCLAVES) continue;
        if (!s_prev_valid[id] || s_prev_task_id[id] != items[k].task->id) {
            s_prev_runtime[id] = total;
            s_prev_task_id[id] = items[k].task->id;
            s_prev_valid[id] = true;
            continue;
        }
        uint32_t delta = (total >= s_prev_runtime[id]) ? (total - s_prev_runtime[id]) : 0;
        s_prev_runtime[id] = total;
        if (delta == 0) continue; // stats disabled or task idle: dormant, honest

        bool exhausted = false;
        mk_port_enter_critical();
        // Re-validate liveness: another trap may have raced us; only the
        // LIVE->FAILED transition fires once.
        if (items[k].desc->state == MK_ENCLAVE_LIVE) {
            if (delta >= items[k].desc->remaining_budget_us) {
                items[k].desc->remaining_budget_us = 0;
                exhausted = true;
            } else {
                items[k].desc->remaining_budget_us -= delta;
            }
        }
        mk_port_exit_critical();
        if (exhausted) {
            // Single trap path for budget overruns: marks FAILED, logs typed
            // fault, suspends the real task (timer context, never the victim).
            mk_enclave_trap(items[k].desc, MK_FAULT_BUDGET_EXHAUSTED, 0x0003, "budget_exhausted");
        }
    }
}

void mk_enclave_dump_status(void) {
    printf("\n=== Nexos-RT V2 Capability Enclaves ===\n");
    printf("ID | NAME        | STATE   | PRIO | TYPE  | CAPS       | BUDGET REM | STKFREE | MUTEX_MASK\n");
    printf("---+-------------+---------+------+-------+------------+------------+---------+-----------\n");
    for (uint32_t i = 0; i < MK_MAX_ENCLAVES; i++) {
        const char* st = "FREE";
        switch (s_enclaves[i].state) {
            case MK_ENCLAVE_RESERVED:   st = "RESERVED"; break;
            case MK_ENCLAVE_LIVE:       st = "LIVE"; break;
            case MK_ENCLAVE_FAILED:     st = "FAILED"; break;
            case MK_ENCLAVE_RECLAIMING: st = "RECLAIM"; break;
            default: break;
        }
        // Memory + budget criteria per enclave: stack watermark + remaining
        // budget. Unbound/absent tasks show dashes (honest, not fabricated).
        char stk[10], bgt[12];
        snprintf(stk, sizeof(stk), "-");
        snprintf(bgt, sizeof(bgt), "-");
        if (s_enclaves[i].state == MK_ENCLAVE_LIVE && s_enclaves[i].task_handle) {
            mk_task_info_t info;
            if (mk_task_get_info(s_enclaves[i].task_handle, &info) == MK_OK) {
                // Clamp: some ports report words-vs-bytes differently; a free
                // count above the configured stack is meaningless — show config.
                unsigned long free = (unsigned long)info.stack_high_watermark;
                unsigned long cfg = (unsigned long)s_enclaves[i].stack_size;
                if (free > cfg) free = cfg;
                snprintf(stk, sizeof(stk), "%lu", free);
            }
            if (s_enclaves[i].budget_us > 0) {
                snprintf(bgt, sizeof(bgt), "%lu",
                         (unsigned long)s_enclaves[i].remaining_budget_us);
            } else {
                snprintf(bgt, sizeof(bgt), "off");
            }
        }
        printf("%2lu | %-11s | %-7s | %4u | %-5s | 0x%08lX | %10s | %7s | 0x%08lX\n",
               (unsigned long)s_enclaves[i].id,
               s_enclaves[i].name[0] ? s_enclaves[i].name : "unassigned",
               st,
               s_enclaves[i].priority,
               s_enclaves[i].type == MK_ENCLAVE_TYPE_RT ? "RT" :
               (s_enclaves[i].type == MK_ENCLAVE_TYPE_INFER ? "INFER" : "SYS"),
               (unsigned long)s_enclaves[i].syscall_mask,
               bgt, stk,
               (unsigned long)s_enclaves[i].held_mutex_mask);
    }
    printf("========================================================================================\n\n");
}

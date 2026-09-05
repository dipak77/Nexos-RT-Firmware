#include "mk_scheduler.h"
#include "mk_kernel.h"
#include "mk_port.h"
#include "mk_task.h"
#include "mk_enclave.h"
#include "mk_fault.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>

static const char* TAG = "MK_SCHED";

#define MK_QUANTUM_TICKS 10

static uint32_t s_ready_bitmap = 0;
static mk_task_t* s_ready_heads[MK_CONFIG_MAX_PRIORITIES] = {0};
static mk_task_t* s_ready_tails[MK_CONFIG_MAX_PRIORITIES] = {0};
static mk_task_t* s_sleep_head = NULL;
static mk_task_t* s_current_task = NULL;

static uint32_t s_ready_count = 0;
static uint64_t s_context_switches = 0;
static uint64_t s_system_ticks = 0;
static uint64_t s_last_tick_us = 0;
static uint32_t s_max_jitter_us = 0;

void mk_scheduler_init(void){
    mk_port_enter_critical();
    s_ready_bitmap = 0;
    memset(s_ready_heads, 0, sizeof(s_ready_heads));
    memset(s_ready_tails, 0, sizeof(s_ready_tails));
    s_sleep_head = NULL;
    s_current_task = NULL;
    s_ready_count = 0;
    s_context_switches = 0;
    s_system_ticks = 0;
    s_last_tick_us = esp_timer_get_time();
    s_max_jitter_us = 0;
    mk_port_exit_critical();
    ESP_LOGI(TAG, "Nexos-RT O(1) Preemptive Bitmap Scheduler initialized (prio 0-%d, tick=%dHz, quantum=%dticks)",
             MK_CONFIG_MAX_PRIORITIES - 1, MK_CONFIG_TICK_HZ, MK_QUANTUM_TICKS);
}

void mk_scheduler_add_ready(mk_task_t* task){
    if(!task) return;
    mk_port_enter_critical();
    uint8_t prio = task->priority;
    if(prio >= MK_CONFIG_MAX_PRIORITIES) prio = MK_CONFIG_MAX_PRIORITIES - 1;
    task->priority = prio;

    // Reset time slice if exhausted
    if(task->time_slice == 0) task->time_slice = MK_QUANTUM_TICKS;

    // V2: Earliest Deadline First (EDF) within same priority
    if(task->deadline_us > 0 && s_ready_heads[prio] != NULL) {
        mk_task_t* cur = s_ready_heads[prio];
        mk_task_t* prev = NULL;
        while(cur && (cur->deadline_us > 0 && cur->deadline_us <= task->deadline_us)) {
            prev = cur;
            cur = cur->next;
        }
        if(!prev) {
            // Insert at head
            task->next = s_ready_heads[prio];
            task->prev = NULL;
            s_ready_heads[prio]->prev = task;
            s_ready_heads[prio] = task;
        } else if(!cur) {
            // Append to tail
            task->prev = s_ready_tails[prio];
            task->next = NULL;
            s_ready_tails[prio]->next = task;
            s_ready_tails[prio] = task;
        } else {
            // Insert in middle
            task->prev = prev;
            task->next = cur;
            prev->next = task;
            cur->prev = task;
        }
    } else {
        // Legacy FIFO / Round-Robin append to tail
        task->prev = s_ready_tails[prio];
        task->next = NULL;
        if(s_ready_tails[prio]) {
            s_ready_tails[prio]->next = task;
        } else {
            s_ready_heads[prio] = task;
        }
        s_ready_tails[prio] = task;
    }

    s_ready_bitmap |= (1u << prio);
    s_ready_count++;
    task->state = MK_TASK_READY;
    s_context_switches++;
    mk_port_exit_critical();
}

void mk_scheduler_remove_ready(mk_task_t* task){
    if(!task) return;
    mk_port_enter_critical();
    uint8_t prio = task->priority;
    if(prio >= MK_CONFIG_MAX_PRIORITIES) prio = MK_CONFIG_MAX_PRIORITIES - 1;

    if(task->prev) task->prev->next = task->next;
    else s_ready_heads[prio] = task->next;

    if(task->next) task->next->prev = task->prev;
    else s_ready_tails[prio] = task->prev;

    task->prev = NULL;
    task->next = NULL;

    if(!s_ready_heads[prio]) {
        s_ready_bitmap &= ~(1u << prio);
    }
    if(s_ready_count) s_ready_count--;
    mk_port_exit_critical();
}

mk_task_t* mk_scheduler_pick_next(void){
    mk_port_enter_critical();
    if(s_ready_bitmap == 0){
        mk_port_exit_critical();
        return NULL;
    }

    // Single instruction bit search: highest priority ready task
    uint8_t top_prio = (uint8_t)(31u - __builtin_clz(s_ready_bitmap));
    if(top_prio >= MK_CONFIG_MAX_PRIORITIES) top_prio = MK_CONFIG_MAX_PRIORITIES - 1;

    mk_task_t* next = s_ready_heads[top_prio];
    if(next && next != s_current_task){
        s_context_switches++;
    }
    s_current_task = next;
    if(next) next->state = MK_TASK_RUNNING;
    mk_port_exit_critical();
    return next;
}

void mk_scheduler_sleep_task(mk_task_t* task, uint32_t ms){
    if(!task) return;
    mk_port_enter_critical();
    mk_scheduler_remove_ready(task);
    task->state = MK_TASK_SLEEPING;
    task->wake_tick = s_system_ticks + (ms ? ms : 1);

    // Insert sorted by wake_tick into sleep list
    if(!s_sleep_head || task->wake_tick < s_sleep_head->wake_tick){
        task->next = s_sleep_head;
        task->prev = NULL;
        if(s_sleep_head) s_sleep_head->prev = task;
        s_sleep_head = task;
    } else {
        mk_task_t* cur = s_sleep_head;
        while(cur->next && cur->next->wake_tick <= task->wake_tick){
            cur = cur->next;
        }
        task->next = cur->next;
        task->prev = cur;
        if(cur->next) cur->next->prev = task;
        cur->next = task;
    }
    mk_port_exit_critical();
}

void mk_scheduler_wake_task(mk_task_t* task){
    if(!task) return;
    mk_port_enter_critical();
    if(task->prev) task->prev->next = task->next;
    else if(s_sleep_head == task) s_sleep_head = task->next;

    if(task->next) task->next->prev = task->prev;
    task->prev = NULL;
    task->next = NULL;

    mk_scheduler_add_ready(task);
    mk_port_exit_critical();
}

void mk_scheduler_block_task(mk_task_t* task){
    if(!task) return;
    mk_port_enter_critical();
    mk_scheduler_remove_ready(task);
    task->state = MK_TASK_BLOCKED;
    mk_port_exit_critical();
}

void mk_scheduler_unblock_task(mk_task_t* task){
    if(!task) return;
    mk_scheduler_add_ready(task);
}

void mk_scheduler_tick(uint64_t now_ms){
    (void)now_ms;
    mk_port_enter_critical();
    s_system_ticks++;

    // Measure tick jitter against 1000us nominal period
    uint64_t now_us = esp_timer_get_time();
    if(s_last_tick_us > 0){
        uint64_t delta = now_us - s_last_tick_us;
        uint32_t jitter = (delta > 1000) ? (uint32_t)(delta - 1000) : (uint32_t)(1000 - delta);
        if(jitter > s_max_jitter_us) s_max_jitter_us = jitter;
    }
    s_last_tick_us = now_us;

    // Wake expired sleeping tasks
    while(s_sleep_head && s_sleep_head->wake_tick <= s_system_ticks){
        mk_task_t* waking = s_sleep_head;
        s_sleep_head = waking->next;
        if(s_sleep_head) s_sleep_head->prev = NULL;
        waking->prev = NULL;
        waking->next = NULL;
        mk_scheduler_add_ready(waking);
    }

    // Accumulate runtime and check round-robin quantum for current task
    if(s_current_task && s_current_task->state == MK_TASK_RUNNING){
        s_current_task->runtime_ticks++;
        if(s_current_task->time_slice > 0){
            s_current_task->time_slice--;
        }

        // NOTE: budget enforcement lives in mk_enclave_sample_budgets() (10Hz,
        // real FreeRTOS run-time deltas). A 1kHz tick cannot attribute execution
        // in the hybrid model (see mk_scheduler_current_task), so no budget
        // accounting happens here — only quantum/RR below. This keeps one
        // owner per mechanism and avoids double-draining.

        // INFER deferral: zero the quantum so RR rotates past this task. Advisory
        // hint only — real preemption stays FreeRTOS priority-driven (hybrid).
        // deadline_us is an absolute esp_timer timestamp; compare time-REMAINING.
        if(s_current_task && s_current_task->enclave_desc){
            mk_enclave_desc_t* enc = (mk_enclave_desc_t*)s_current_task->enclave_desc;
            if(enc->type == MK_ENCLAVE_TYPE_INFER){
                // Check if any higher/equal priority RT task is waiting with imminent deadline
                for(int p = MK_CONFIG_MAX_PRIORITIES - 1; p >= (int)s_current_task->priority; p--){
                    if(s_ready_heads[p] && s_ready_heads[p]->enclave_desc){
                        mk_enclave_desc_t* top_enc = (mk_enclave_desc_t*)s_ready_heads[p]->enclave_desc;
                        if(top_enc->type == MK_ENCLAVE_TYPE_RT && top_enc->deadline_us > 0){
                            uint64_t dl = (uint64_t)top_enc->deadline_us;
                            uint64_t remaining = (dl > now_us) ? (dl - now_us) : 0;
                            if(remaining < 5000){
                                // Yield infer task in favor of imminent RT deadline
                                s_current_task->time_slice = 0;
                                break;
                            }
                        }
                    }
                }
            }
        }

        // Round-robin quantum expired: rotate within same priority.
        // s_current_task may be NULL here (budget trap suspended it above).
        if(s_current_task && s_current_task->time_slice == 0){
            s_current_task->time_slice = MK_QUANTUM_TICKS;
            uint8_t prio = s_current_task->priority;
            if(prio < MK_CONFIG_MAX_PRIORITIES && s_ready_heads[prio] != s_ready_tails[prio]){
                // Multiple tasks at this priority: rotate head to tail
                mk_task_t* head = s_ready_heads[prio];
                if(head == s_current_task && head->next){
                    s_ready_heads[prio] = head->next;
                    s_ready_heads[prio]->prev = NULL;

                    s_ready_tails[prio]->next = head;
                    head->prev = s_ready_tails[prio];
                    head->next = NULL;
                    s_ready_tails[prio] = head;
                }
            }
        }
    }
    mk_port_exit_critical();

    // 10Hz budget sampling against REAL FreeRTOS run-time counters (see
    // mk_enclave_sample_budgets). Kept out of the critical section: the port
    // walk suspends the scheduler internally. Dormant when the toolchain has
    // run-time stats disabled (deltas read 0) or no enclave carries a budget.
    if((s_system_ticks % 100) == 0){
        mk_enclave_sample_budgets();
        mk_task_reap();
    }
}

uint32_t mk_scheduler_ready_count(void){ return s_ready_count; }
uint32_t mk_scheduler_get_max_jitter_us(void){ return s_max_jitter_us; }
uint64_t mk_scheduler_get_context_switches(void){ return s_context_switches; }
// Prod-grade: execution is FreeRTOS-driven (hybrid), so the cached pointer set
// at task entry goes stale after every real switch. Resolve the true running
// task from the port handle on every query; fall back to the cache only when
// the port lookup finds no live Nexos-RT task (early boot / foreign caller).
// This is what makes PI attribution, budget accounting and SVC capability
// checks correct under the hybrid execution model.
mk_task_t* mk_scheduler_current_task(void){
    // Return live registered task, or NULL if foreign unmanaged thread (A5)
    return mk_task_self();
}
void mk_scheduler_set_current_task(mk_task_t* task){ s_current_task = task; }

#if defined(ESP_PLATFORM)
#include "esp_timer.h"
static esp_timer_handle_t s_tick_timer = NULL;
static void mk_scheduler_tick_cb(void* arg){
    (void)arg;
    mk_scheduler_tick(0);
}
#endif

void mk_scheduler_start_tick(void){
#if defined(ESP_PLATFORM)
    if(s_tick_timer) return;
    const esp_timer_create_args_t args = {
        .callback = mk_scheduler_tick_cb,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "nexos_tick",
    };
    if(esp_timer_create(&args, &s_tick_timer) != ESP_OK){ s_tick_timer = NULL; return; }
    esp_timer_start_periodic(s_tick_timer, 1000);
#else
    (void)0;
#endif
}

void mk_scheduler_stop_tick(void){
#if defined(ESP_PLATFORM)
    if(!s_tick_timer) return;
    esp_timer_stop(s_tick_timer);
    esp_timer_delete(s_tick_timer);
    s_tick_timer = NULL;
#endif
}

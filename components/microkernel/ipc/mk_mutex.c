#include "mk_mutex.h"
#include "mk_chip_port.h"
#include "mk_config.h"
#include "mk_port.h"
#include "mk_scheduler.h"
#include <stdlib.h>
#include <string.h>
#include "esp_timer.h"
#include "esp_heap_caps.h"

#if MK_NATIVE_KERNEL
// Native Priority-Inheritance Mutex
struct mk_mutex {
    volatile int locked;
    uint32_t recursion_count;
    char name[32];
    void* owner;
    mk_task_t* owner_tcb;
    uint8_t original_prio;
};

mk_mutex_t* mk_mutex_create(const char* name){
    mk_mutex_t* m = (mk_mutex_t*)heap_caps_malloc(sizeof(mk_mutex_t), MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
    if(!m) m = (mk_mutex_t*)malloc(sizeof(mk_mutex_t));
    if(!m) return NULL;
    memset(m, 0, sizeof(*m));
    m->locked = 0;
    m->recursion_count = 0;
    m->owner = NULL;
    m->owner_tcb = NULL;
    m->original_prio = 0;
    if(name) strncpy(m->name, name, sizeof(m->name)-1);
    return m;
}

mk_status_t mk_mutex_delete(mk_mutex_t* mutex){
    if(!mutex) return MK_ERR_INVALID;
    heap_caps_free(mutex);
    return MK_OK;
}

mk_status_t mk_mutex_lock(mk_mutex_t* mutex, uint32_t timeout_ms){
    if(!mutex) return MK_ERR_INVALID;
    void* self = mk_port_task_self();
    mk_task_t* self_tcb = mk_scheduler_current_task();

    mk_port_enter_critical();
    // Fast path 1: Uncontended
    if(!mutex->locked){
        mutex->locked = 1;
        mutex->recursion_count = 1;
        mutex->owner = self;
        mutex->owner_tcb = self_tcb;
        if(self_tcb) mutex->original_prio = self_tcb->priority;
        mk_port_exit_critical();
        return MK_OK;
    }

    // Fast path 2: Recursive acquisition by owner
    if(mutex->owner == self){
        mutex->recursion_count++;
        mk_port_exit_critical();
        return MK_OK;
    }

    if(timeout_ms == 0){
        mk_port_exit_critical();
        return MK_ERR_TIMEOUT;
    }

    // Priority Inheritance: Boost owner priority if current task has higher priority
    if(self_tcb && mutex->owner_tcb && self_tcb->priority > mutex->owner_tcb->priority){
        mutex->owner_tcb->priority = self_tcb->priority;
        mk_scheduler_remove_ready(mutex->owner_tcb);
        mk_scheduler_add_ready(mutex->owner_tcb);
    }
    mk_port_exit_critical();

    // Contended wait loop with timeout
    uint64_t until = (timeout_ms == 0xFFFFFFFF) ? UINT64_MAX : esp_timer_get_time() + (uint64_t)timeout_ms * 1000;
    while(1){
        mk_port_enter_critical();
        if(!mutex->locked){
            mutex->locked = 1;
            mutex->recursion_count = 1;
            mutex->owner = self;
            mutex->owner_tcb = self_tcb;
            if(self_tcb) mutex->original_prio = self_tcb->priority;
            mk_port_exit_critical();
            return MK_OK;
        }

        // Re-apply priority boost in case owner changed
        if(self_tcb && mutex->owner_tcb && self_tcb->priority > mutex->owner_tcb->priority){
            mutex->owner_tcb->priority = self_tcb->priority;
            mk_scheduler_remove_ready(mutex->owner_tcb);
            mk_scheduler_add_ready(mutex->owner_tcb);
        }
        mk_port_exit_critical();

        if(esp_timer_get_time() >= until) return MK_ERR_TIMEOUT;
        mk_port_yield();
        mk_port_delay_ms(1);
    }
}

mk_status_t mk_mutex_try_lock(mk_mutex_t* mutex){
    return mk_mutex_lock(mutex, 0);
}

mk_status_t mk_mutex_unlock(mk_mutex_t* mutex){
    if(!mutex) return MK_ERR_INVALID;
    void* self = mk_port_task_self();

    mk_port_enter_critical();
    if(!mutex->locked || mutex->owner != self){
        mk_port_exit_critical();
        return MK_ERR_INVALID;
    }

    if(mutex->recursion_count > 1){
        mutex->recursion_count--;
        mk_port_exit_critical();
        return MK_OK;
    }

    // Restore owner base priority if boosted
    if(mutex->owner_tcb){
        uint8_t target_prio = mutex->owner_tcb->base_priority ? mutex->owner_tcb->base_priority : mutex->original_prio;
        if(mutex->owner_tcb->priority != target_prio){
            mutex->owner_tcb->priority = target_prio;
            mk_scheduler_remove_ready(mutex->owner_tcb);
            mk_scheduler_add_ready(mutex->owner_tcb);
        }
    }

    mutex->locked = 0;
    mutex->recursion_count = 0;
    mutex->owner = NULL;
    mutex->owner_tcb = NULL;
    mk_port_exit_critical();
    mk_port_yield();
    return MK_OK;
}

bool mk_mutex_is_locked(mk_mutex_t* mutex){ return mutex ? mutex->locked != 0 : false; }
const char* mk_mutex_get_name(mk_mutex_t* mutex){ return mutex ? mutex->name : ""; }
void* mk_mutex_get_owner(mk_mutex_t* mutex){ return mutex ? mutex->owner : NULL; }

#else
// Legacy shim fallback
struct mk_mutex { SemaphoreHandle_t handle; char name[32]; };
mk_mutex_t* mk_mutex_create(const char* name){
    mk_mutex_t* m = (mk_mutex_t*)malloc(sizeof(mk_mutex_t));
    if(!m) return NULL;
    m->handle = xSemaphoreCreateMutex();
    if(!m->handle){ free(m); return NULL; }
    if(name) strncpy(m->name, name, sizeof(m->name)-1);
    return m;
}
mk_status_t mk_mutex_delete(mk_mutex_t* mutex){
    if(!mutex) return MK_ERR_INVALID;
    vSemaphoreDelete(mutex->handle);
    free(mutex);
    return MK_OK;
}
mk_status_t mk_mutex_lock(mk_mutex_t* mutex, uint32_t timeout_ms){
    if(!mutex) return MK_ERR_INVALID;
    TickType_t ticks = timeout_ms==0xFFFFFFFF ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    if(xSemaphoreTake(mutex->handle, ticks)==pdTRUE) return MK_OK;
    return MK_ERR_TIMEOUT;
}
mk_status_t mk_mutex_try_lock(mk_mutex_t* mutex){
    return mk_mutex_lock(mutex, 0);
}
mk_status_t mk_mutex_unlock(mk_mutex_t* mutex){
    if(!mutex) return MK_ERR_INVALID;
    if(xSemaphoreGive(mutex->handle)==pdTRUE) return MK_OK;
    return MK_ERR_INVALID;
}
bool mk_mutex_is_locked(mk_mutex_t* mutex){
    return mutex && uxSemaphoreGetCount(mutex->handle) == 0;
}
const char* mk_mutex_get_name(mk_mutex_t* mutex){ return mutex ? mutex->name : ""; }
void* mk_mutex_get_owner(mk_mutex_t* mutex){ return mutex && mutex->handle ? (void*)xSemaphoreGetMutexHolder(mutex->handle) : NULL; }
#endif

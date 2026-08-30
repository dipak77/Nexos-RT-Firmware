#include "mk_task.h"
#include "mk_kernel.h"
#include "mk_scheduler.h"
#include "esp_log.h"
#include "mk_port.h"
#include <string.h>
#include <stdlib.h>

static const char* TAG = "NEXOS_TASK";

typedef enum { SLOT_FREE = 0, SLOT_RESERVED, SLOT_LIVE, SLOT_DELETING } mk_slot_state_t;

typedef struct mk_task_internal {
    mk_task_t public_tcb;
    mk_port_task_handle_t port_handle;
    mk_task_entry_t entry;
    void* arg;
    char name_storage[32];
    mk_slot_state_t slot_state;
} mk_task_internal_t;

static mk_task_internal_t s_tasks[MK_CONFIG_MAX_TASKS];
static uint32_t s_task_count = 0;
static uint32_t s_next_id = 1;


static void mk_port_task_wrapper(void* arg){
    mk_task_internal_t* internal = (mk_task_internal_t*)arg;
    mk_wait_start();
    internal->public_tcb.state = MK_TASK_RUNNING;
    if (internal->entry) internal->entry(internal->arg);
    mk_task_delete(&internal->public_tcb);
#if MK_NATIVE_KERNEL
    // Native cooperative task returned — scheduler will pick next
    mk_port_yield();
#endif
}

mk_task_handle_t mk_task_create(const char* name, mk_task_entry_t entry, void* arg, void* stack, size_t stack_size, uint8_t priority){
    mk_task_config_t cfg = {0};
    cfg.name = name;
    cfg.priority = priority;
    cfg.stack_size = stack_size ? stack_size : 4096;
    cfg.stack_base = stack;
    cfg.core_affinity = 1;
    return mk_task_create_ext(&cfg, entry, arg);
}

mk_task_handle_t mk_task_create_ext(const mk_task_config_t* config, mk_task_entry_t entry, void* arg){
    if(!config || !entry) return NULL;
    if(!mk_is_initialized()) return NULL;

    mk_port_enter_critical();
    int idx = -1;
    for(int i=0;i<MK_CONFIG_MAX_TASKS;i++){
        if(s_tasks[i].slot_state == SLOT_FREE){ idx=i; break; }
    }
    if(idx<0){
        mk_port_exit_critical();
        return NULL;
    }
    mk_task_internal_t* internal = &s_tasks[idx];
    memset(internal, 0, sizeof(*internal));
    internal->slot_state = SLOT_RESERVED;
    internal->entry = entry;
    internal->arg = arg;
    internal->public_tcb.id = s_next_id++;
    internal->public_tcb.priority = config->priority;
    internal->public_tcb.state = MK_TASK_READY;
    internal->public_tcb.stack_size = config->stack_size;
    internal->public_tcb.stack_base = NULL;
    internal->public_tcb.stack_pointer = NULL;
    strncpy(internal->name_storage, config->name ? config->name : "mk_task", sizeof(internal->name_storage)-1);
    internal->name_storage[sizeof(internal->name_storage)-1] = 0;
    internal->public_tcb.name = internal->name_storage;
    mk_port_exit_critical();

    uint32_t port_prio = mk_map_port_priority(config->priority);
    int core = config->core_affinity >= 0 ? config->core_affinity : 1;
    // Unified port — core never touches FreeRTOS directly. Port decides native vs shim.
    mk_port_task_handle_t h = mk_port_task_create(internal->name_storage, mk_port_task_wrapper, internal, config->stack_size, config->priority, core);
    mk_port_enter_critical();
    if(!h){
        memset(internal, 0, sizeof(*internal));
        internal->slot_state = SLOT_FREE;
        mk_port_exit_critical();
        ESP_LOGE(TAG, "task create failed for %s", config->name ? config->name : "?");
        return NULL;
    }
    internal->port_handle = h;
    internal->slot_state = SLOT_LIVE;
    s_task_count++;
    mk_port_exit_critical();
    ESP_LOGI(TAG, "Task created [%s]: %s id=%lu prio=%d port_prio=%lu core=%d",
             MK_NATIVE_KERNEL?"NATIVE":"SHIM",
             internal->name_storage, (unsigned long)internal->public_tcb.id,
             config->priority, (unsigned long)port_prio, core);
    return &internal->public_tcb;
}

mk_status_t mk_task_delete(mk_task_handle_t task){
    if(!task) task = mk_task_self();
    if(!task) return MK_ERR_INVALID;
    mk_port_task_handle_t h = NULL;
    bool self = false;
    mk_port_enter_critical();
    for(int i=0;i<MK_CONFIG_MAX_TASKS;i++){
        if(&s_tasks[i].public_tcb == task){
            if (s_tasks[i].slot_state != SLOT_LIVE) {
                mk_port_exit_critical();
                return MK_ERR_BAD_STATE;
            }
            s_tasks[i].slot_state = SLOT_DELETING;
            h = s_tasks[i].port_handle;
            self = (h != NULL && h == mk_port_task_self());
            s_tasks[i].public_tcb.state = MK_TASK_DELETED;
            s_tasks[i].port_handle = NULL;
            if(s_task_count) s_task_count--;
            s_tasks[i].slot_state = SLOT_FREE;
            mk_port_exit_critical();
            if(h){
                mk_port_task_delete(h);
            }
            return MK_OK;
        }
    }
    mk_port_exit_critical();
    return MK_ERR_NOT_FOUND;
}

mk_task_handle_t mk_task_self(void){
    mk_port_task_handle_t self = mk_port_task_self();
    mk_task_handle_t found = NULL;
    mk_port_enter_critical();
    for(int i=0;i<MK_CONFIG_MAX_TASKS;i++){
        if(s_tasks[i].slot_state == SLOT_LIVE && s_tasks[i].port_handle == self) {
            found = &s_tasks[i].public_tcb;
            break;
        }
    }
    mk_port_exit_critical();
    return found;
}

const char* mk_task_get_name(mk_task_handle_t task){
    if(!task) return "unknown";
    return task->name;
}
mk_task_state_t mk_task_get_state(mk_task_handle_t task){
    if(!task) return MK_TASK_SUSPENDED;
    return task->state;
}
mk_status_t mk_task_get_info(mk_task_handle_t task, mk_task_info_t* info){
    if(!task || !info) return MK_ERR_INVALID;
    memset(info, 0, sizeof(*info));
    mk_port_enter_critical();
    info->id = task->id;
    strncpy(info->name, task->name ? task->name : "", sizeof(info->name)-1);
    info->state = task->state;
    info->priority = task->priority;
    info->stack_size = task->stack_size;
    mk_port_task_handle_t h = NULL;
    for(int i=0;i<MK_CONFIG_MAX_TASKS;i++){
        if(&s_tasks[i].public_tcb == task) { h = s_tasks[i].port_handle; break; }
    }
    mk_port_exit_critical();
    if(h){
        info->stack_high_watermark = mk_port_task_get_stack_watermark(h);
    }
    return MK_OK;
}
uint32_t mk_task_count(void){ return s_task_count; }

mk_status_t mk_task_suspend(mk_task_handle_t task){
    for(int i=0;i<MK_CONFIG_MAX_TASKS;i++){
        if(&s_tasks[i].public_tcb == task && s_tasks[i].port_handle){
            mk_port_task_suspend((mk_port_task_handle_t)s_tasks[i].port_handle);
            task->state = MK_TASK_SUSPENDED;
            return MK_OK;
        }
    }
    return MK_ERR_NOT_FOUND;
}
mk_status_t mk_task_resume(mk_task_handle_t task){
    for(int i=0;i<MK_CONFIG_MAX_TASKS;i++){
        if(&s_tasks[i].public_tcb == task && s_tasks[i].port_handle){
            mk_port_task_resume((mk_port_task_handle_t)s_tasks[i].port_handle);
            task->state = MK_TASK_READY;
            return MK_OK;
        }
    }
    return MK_ERR_NOT_FOUND;
}

#include "mk_config.h"
#if MK_NATIVE_KERNEL
// Include FreeRTOS FIRST so that when mk_port.h pulls native defs it sees
// portMUX_TYPE already defined and skips its uint32_t shim (avoids redefinition).
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "mk_port.h"
// Pure Nexos-RT native port — header is FreeRTOS-free for core; this .c is the ONLY
// place that may touch FreeRTOS until mk_context.S replaces it.
// Phase 1: delegate to FreeRTOS hidden shim behind the port.
// Phase 2 (next): Xtensa window spill + GPTimer preempt (mk_context.S).
#include "esp_cpu.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include <string.h>
#include <stdlib.h>

static const char* TAG="NEXOS_NATIVE";
static portMUX_TYPE s_port_mux = portMUX_INITIALIZER_UNLOCKED;

void mk_port_enter_critical(void){ portENTER_CRITICAL(&s_port_mux); }
void mk_port_exit_critical(void){ portEXIT_CRITICAL(&s_port_mux); }
void mk_port_yield(void){ taskYIELD(); }
void mk_port_delay_ms(uint32_t ms){
    if(ms==0){ taskYIELD(); return; }
    vTaskDelay(pdMS_TO_TICKS(ms));
}
uint64_t mk_port_get_time_us(void){ return esp_timer_get_time(); }
int mk_port_get_core_id(void){ return (int)xPortGetCoreID(); }

mk_port_task_handle_t mk_port_task_create(const char* name, mk_port_task_entry_t entry, void* arg,
                                          uint32_t stack_bytes, uint8_t mk_prio, int core_affinity){
    extern uint32_t mk_map_port_priority(uint8_t);
    uint32_t port_prio = mk_map_port_priority(mk_prio);
    TaskHandle_t h=NULL;
    BaseType_t ret;
    if(core_affinity>=0) ret = xTaskCreatePinnedToCore((TaskFunction_t)entry, name?name:"nexos", stack_bytes, arg, port_prio, &h, core_affinity);
    else ret = xTaskCreate((TaskFunction_t)entry, name?name:"nexos", stack_bytes, arg, port_prio, &h);
    if(ret!=pdPASS) return NULL;
    ESP_LOGI(TAG, "native-port task %s prio=%u->%lu core=%d stack=%u [via hidden FreeRTOS shim]", name, mk_prio, (unsigned long)port_prio, core_affinity, (unsigned)stack_bytes);
    return (mk_port_task_handle_t)h;
}
void mk_port_task_delete(mk_port_task_handle_t h){
    if(!h) h = (mk_port_task_handle_t)xTaskGetCurrentTaskHandle();
    vTaskDelete((TaskHandle_t)h);
}
mk_port_task_handle_t mk_port_task_self(void){ return (mk_port_task_handle_t)xTaskGetCurrentTaskHandle(); }
uint32_t mk_port_task_get_stack_watermark(mk_port_task_handle_t h){
    return uxTaskGetStackHighWaterMark((TaskHandle_t)h)*4;
}
void mk_port_task_suspend(mk_port_task_handle_t h){ vTaskSuspend((TaskHandle_t)h); }
void mk_port_task_resume(mk_port_task_handle_t h){ vTaskResume((TaskHandle_t)h); }
// Weak: only present when the toolchain enables INCLUDE_pxTaskGetStackStart.
extern uint8_t* pxTaskGetStackStart(TaskHandle_t xTask) __attribute__((weak));
bool mk_port_task_stack_base(mk_port_task_handle_t h, uintptr_t *out_base){
    if(!h || !out_base) return false;
    if(pxTaskGetStackStart == NULL) return false;
    uint8_t* b = pxTaskGetStackStart((TaskHandle_t)h);
    if(!b) return false;
    *out_base = (uintptr_t)b;
    return true;
}
extern eTaskState eTaskGetState(TaskHandle_t xTask) __attribute__((weak));
bool mk_port_task_is_alive(mk_port_task_handle_t h){
    if(!h) return false;
    if(eTaskGetState != NULL){
        return (eTaskGetState((TaskHandle_t)h) != eDeleted);
    }
    return false;
}
// Static table (no per-call alloc): sampler runs 10Hz, 4 enclaves max.
// Weak-linked: Arduino cores without trace facility leave this NULL, and the
// sampler honestly reports stats-unavailable instead of failing the link.
extern UBaseType_t uxTaskGetSystemState(TaskStatus_t *pxTaskStatusArray, UBaseType_t uxArraySize, uint32_t *pulTotalRunTime) __attribute__((weak));
static TaskStatus_t s_runtime_table[40];
bool mk_port_task_runtime(mk_port_task_handle_t h, uint32_t *out_ticks){
    if(!h || !out_ticks) return false;
    if(uxTaskGetSystemState == NULL) return false; // toolchain w/o trace: dormant
    UBaseType_t n = uxTaskGetNumberOfTasks();
    if(n == 0 || n > 40) return false;
    UBaseType_t got = uxTaskGetSystemState(s_runtime_table, 40, NULL);
    for(UBaseType_t i = 0; i < got; i++){
        if(s_runtime_table[i].xHandle == (TaskHandle_t)h){
            *out_ticks = s_runtime_table[i].ulRunTimeCounter;
            return true;
        }
    }
    return false;
}
void mk_port_task_set_priority(mk_port_task_handle_t h, uint8_t mk_prio){
    if(!h) return;
    extern uint32_t mk_map_port_priority(uint8_t);
    vTaskPrioritySet((TaskHandle_t)h, mk_map_port_priority(mk_prio));
}

typedef struct { uint32_t bits; portMUX_TYPE lock; } native_event_t;
mk_port_event_group_handle_t mk_port_event_create(const char* name){
    (void)name;
    native_event_t* e=heap_caps_malloc(sizeof(*e), MALLOC_CAP_INTERNAL);
    if(e){
        // Proper struct init for spinlock (portMUX_INITIALIZER_UNLOCKED is {.owner=..., .count=0})
        *e = (native_event_t){ .bits=0, .lock=portMUX_INITIALIZER_UNLOCKED };
    }
    return (mk_port_event_group_handle_t)e;
}
void mk_port_event_delete(mk_port_event_group_handle_t h){ heap_caps_free(h); }
uint32_t mk_port_event_set(mk_port_event_group_handle_t h, uint32_t bits){ if(!h) return 0; native_event_t* e=(native_event_t*)h; taskENTER_CRITICAL((portMUX_TYPE*)&e->lock); e->bits|=bits; uint32_t r=e->bits; taskEXIT_CRITICAL((portMUX_TYPE*)&e->lock); return r; }
uint32_t mk_port_event_clear(mk_port_event_group_handle_t h, uint32_t bits){ if(!h) return 0; native_event_t* e=(native_event_t*)h; taskENTER_CRITICAL((portMUX_TYPE*)&e->lock); e->bits&=~bits; uint32_t r=e->bits; taskEXIT_CRITICAL((portMUX_TYPE*)&e->lock); return r; }
uint32_t mk_port_event_wait(mk_port_event_group_handle_t h, uint32_t bits, bool clear_on_exit, bool wait_all, uint32_t timeout_ms){
    native_event_t* e=(native_event_t*)h; if(!e) return 0;
    TickType_t ticks = (timeout_ms==0xFFFFFFFF)?portMAX_DELAY:pdMS_TO_TICKS(timeout_ms);
    // Use FreeRTOS event wait hidden — still native header-free
    // To keep native event semantics but leverage FreeRTOS for blocking, create temp EventGroup per wait
    // Simpler: poll with yield
    uint64_t until = (timeout_ms==0xFFFFFFFF)? UINT64_MAX : esp_timer_get_time() + (uint64_t)timeout_ms*1000;
    while(1){
        taskENTER_CRITICAL((portMUX_TYPE*)&e->lock);
        uint32_t cur=e->bits;
        bool ok = wait_all ? ((cur & bits)==bits) : ((cur & bits)!=0);
        if(ok){
            uint32_t ret=cur & bits;
            if(clear_on_exit) e->bits &= ~bits;
            taskEXIT_CRITICAL((portMUX_TYPE*)&e->lock);
            return ret;
        }
        taskEXIT_CRITICAL((portMUX_TYPE*)&e->lock);
        if(esp_timer_get_time() >= until) return e->bits & bits;
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}
uint32_t mk_port_event_get(mk_port_event_group_handle_t h){ if(!h) return 0; native_event_t* e=(native_event_t*)h; return e->bits; }

void mk_native_scheduler_run(void){ /* placeholder — preemptive mk_context.S will hook GPTimer */ }

#endif // MK_NATIVE_KERNEL


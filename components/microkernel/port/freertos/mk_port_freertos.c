#include "mk_config.h"
#if !MK_NATIVE_KERNEL
#include "mk_port.h"
// Legacy shim backend — wraps FreeRTOS but ONLY this file includes freertos/
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_timer.h"

static portMUX_TYPE s_port_mux = portMUX_INITIALIZER_UNLOCKED;
void mk_port_enter_critical(void){ portENTER_CRITICAL(&s_port_mux); }
void mk_port_exit_critical(void){ portEXIT_CRITICAL(&s_port_mux); }
void mk_port_yield(void){ taskYIELD(); }
void mk_port_delay_ms(uint32_t ms){ if(ms==0){ taskYIELD(); return; } vTaskDelay(pdMS_TO_TICKS(ms)); }
uint64_t mk_port_get_time_us(void){ return esp_timer_get_time(); }
int mk_port_get_core_id(void){ return xPortGetCoreID(); }

mk_port_task_handle_t mk_port_task_create(const char* name, mk_port_task_entry_t entry, void* arg,
                                          uint32_t stack_bytes, uint8_t mk_prio, int core_affinity){
    uint32_t port_prio = 8;
    // map mk prio 1..7 -> port 2..16 as before
    extern uint32_t mk_map_port_priority(uint8_t);
    port_prio = mk_map_port_priority(mk_prio);
    TaskHandle_t h=NULL;
    BaseType_t ret;
    if(core_affinity>=0){
        ret = xTaskCreatePinnedToCore((TaskFunction_t)entry, name, stack_bytes, arg, port_prio, &h, core_affinity);
    }else{
        ret = xTaskCreate((TaskFunction_t)entry, name, stack_bytes, arg, port_prio, &h);
    }
    return (ret==pdPASS)? (mk_port_task_handle_t)h : NULL;
}
void mk_port_task_delete(mk_port_task_handle_t h){
    if(!h) h = (mk_port_task_handle_t)xTaskGetCurrentTaskHandle();
    vTaskDelete((TaskHandle_t)h);
}
mk_port_task_handle_t mk_port_task_self(void){ return (mk_port_task_handle_t)xTaskGetCurrentTaskHandle(); }
uint32_t mk_port_task_get_stack_watermark(mk_port_task_handle_t h){ return uxTaskGetStackHighWaterMark((TaskHandle_t)h)*4; }
void mk_port_task_suspend(mk_port_task_handle_t h){ vTaskSuspend((TaskHandle_t)h); }
void mk_port_task_resume(mk_port_task_handle_t h){ vTaskResume((TaskHandle_t)h); }
void mk_port_task_set_priority(mk_port_task_handle_t h, uint8_t mk_prio){
    if(!h) return;
    extern uint32_t mk_map_port_priority(uint8_t);
    vTaskPrioritySet((TaskHandle_t)h, mk_map_port_priority(mk_prio));
}

mk_port_event_group_handle_t mk_port_event_create(const char* name){ (void)name; return (mk_port_event_group_handle_t)xEventGroupCreate(); }
void mk_port_event_delete(mk_port_event_group_handle_t h){ vEventGroupDelete((EventGroupHandle_t)h); }
uint32_t mk_port_event_set(mk_port_event_group_handle_t h, uint32_t bits){ return xEventGroupSetBits((EventGroupHandle_t)h, bits); }
uint32_t mk_port_event_clear(mk_port_event_group_handle_t h, uint32_t bits){ return xEventGroupClearBits((EventGroupHandle_t)h, bits); }
uint32_t mk_port_event_wait(mk_port_event_group_handle_t h, uint32_t bits, bool clear_on_exit, bool wait_all, uint32_t timeout_ms){
    TickType_t ticks = (timeout_ms==0xFFFFFFFF)?portMAX_DELAY:pdMS_TO_TICKS(timeout_ms);
    return xEventGroupWaitBits((EventGroupHandle_t)h, bits, clear_on_exit?pdTRUE:pdFALSE, wait_all?pdTRUE:pdFALSE, ticks);
}
uint32_t mk_port_event_get(mk_port_event_group_handle_t h){ return xEventGroupGetBits((EventGroupHandle_t)h); }
#endif // !MK_NATIVE_KERNEL

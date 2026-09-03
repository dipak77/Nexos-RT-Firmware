#include "mk_semaphore.h"
#include "mk_chip_port.h"
#include "mk_config.h"
#include <stdlib.h>
#include <string.h>
#include "mk_port.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"

#if MK_NATIVE_KERNEL
struct mk_semaphore {
    uint32_t count;
    uint32_t max;
    portMUX_TYPE lock;
};

mk_semaphore_t* mk_semaphore_create(uint32_t max_count, uint32_t initial_count, const char* name){
    (void)name;
    if(max_count == 0) return NULL;
    mk_semaphore_t* s = (mk_semaphore_t*)heap_caps_malloc(sizeof(*s), MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
    if(!s) s = (mk_semaphore_t*)malloc(sizeof(*s));
    if(!s) return NULL;
    s->max = max_count;
    s->count = (initial_count <= max_count) ? initial_count : max_count;
    memset((void*)&s->lock, 0, sizeof(s->lock));
    portMUX_INITIALIZE(&s->lock);
    return s;
}

mk_status_t mk_semaphore_delete(mk_semaphore_t* sem){
    if(!sem) return MK_ERR_INVALID;
    heap_caps_free(sem);
    return MK_OK;
}

mk_status_t mk_semaphore_take(mk_semaphore_t* sem, uint32_t timeout_ms){
    if(!sem) return MK_ERR_INVALID;
    uint64_t until = (timeout_ms == 0xFFFFFFFF) ? UINT64_MAX : esp_timer_get_time() + (uint64_t)timeout_ms * 1000;
    while(1){
        taskENTER_CRITICAL(&sem->lock);
        if(sem->count > 0){
            sem->count--;
            taskEXIT_CRITICAL(&sem->lock);
            return MK_OK;
        }
        taskEXIT_CRITICAL(&sem->lock);
        if(timeout_ms == 0 || esp_timer_get_time() >= until) return MK_ERR_TIMEOUT;
        mk_port_yield();
        mk_port_delay_ms(1);
    }
}

mk_status_t mk_semaphore_give(mk_semaphore_t* sem){
    if(!sem) return MK_ERR_INVALID;
    taskENTER_CRITICAL(&sem->lock);
    if(sem->count < sem->max){
        sem->count++;
    } else {
        taskEXIT_CRITICAL(&sem->lock);
        return MK_ERR_INVALID;
    }
    taskEXIT_CRITICAL(&sem->lock);
    mk_port_yield();
    return MK_OK;
}

uint32_t mk_semaphore_get_count(mk_semaphore_t* sem){
    if(!sem) return 0;
    taskENTER_CRITICAL(&sem->lock);
    uint32_t c = sem->count;
    taskEXIT_CRITICAL(&sem->lock);
    return c;
}

#else
struct mk_semaphore { SemaphoreHandle_t handle; };
mk_semaphore_t* mk_semaphore_create(uint32_t max_count, uint32_t initial_count, const char* name){
    mk_semaphore_t* s = (mk_semaphore_t*)malloc(sizeof(mk_semaphore_t));
    if(!s) return NULL;
    s->handle = xSemaphoreCreateCounting(max_count, initial_count);
    if(!s->handle){ free(s); return NULL; }
    return s;
}
mk_status_t mk_semaphore_delete(mk_semaphore_t* sem){ if(!sem) return MK_ERR_INVALID; vSemaphoreDelete(sem->handle); free(sem); return MK_OK; }
mk_status_t mk_semaphore_take(mk_semaphore_t* sem, uint32_t timeout_ms){
    if(!sem) return MK_ERR_INVALID;
    TickType_t ticks = timeout_ms==0xFFFFFFFF ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTake(sem->handle, ticks)==pdTRUE ? MK_OK : MK_ERR_TIMEOUT;
}
mk_status_t mk_semaphore_give(mk_semaphore_t* sem){
    if(!sem) return MK_ERR_INVALID;
    return xSemaphoreGive(sem->handle)==pdTRUE ? MK_OK : MK_ERR_INVALID;
}
uint32_t mk_semaphore_get_count(mk_semaphore_t* sem){ if(!sem) return 0; return uxSemaphoreGetCount(sem->handle); }
#endif

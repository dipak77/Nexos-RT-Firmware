#include "mk_semaphore.h"
#include "mk_chip_port.h"
#include <stdlib.h>
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

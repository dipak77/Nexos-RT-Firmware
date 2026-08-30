#include "mk_mutex.h"
#include "mk_chip_port.h"
#include <stdlib.h>
#include <string.h>
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
mk_status_t mk_mutex_unlock(mk_mutex_t* mutex){
    if(!mutex) return MK_ERR_INVALID;
    if(xSemaphoreGive(mutex->handle)==pdTRUE) return MK_OK;
    return MK_ERR_INVALID;
}
bool mk_mutex_is_locked(mk_mutex_t* mutex){ return false; }

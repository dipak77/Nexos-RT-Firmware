#include "mk_mutex.h"
#include "mk_chip_port.h"
#include "mk_config.h"
#include "mk_port.h"
#include <stdlib.h>
#include <string.h>
#include "esp_timer.h"
#include "esp_heap_caps.h"

#if MK_NATIVE_KERNEL
// Native mutex — spinlock + owner, PI not yet (future)
struct mk_mutex {
    volatile int locked;
    char name[32];
    void* owner;
};
mk_mutex_t* mk_mutex_create(const char* name){
    mk_mutex_t* m = (mk_mutex_t*)heap_caps_malloc(sizeof(mk_mutex_t), MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
    if(!m) m = malloc(sizeof(mk_mutex_t));
    if(!m) return NULL;
    memset(m,0,sizeof(*m));
    m->locked=0;
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
    uint64_t until = (timeout_ms==0xFFFFFFFF)? UINT64_MAX : esp_timer_get_time() + (uint64_t)timeout_ms*1000;
    while(1){
        mk_port_enter_critical();
        if(!mutex->locked){
            mutex->locked=1;
            mutex->owner=mk_port_task_self();
            mk_port_exit_critical();
            return MK_OK;
        }
        mk_port_exit_critical();
        if(esp_timer_get_time() >= until) return MK_ERR_TIMEOUT;
        mk_port_yield();
        // small backoff to avoid starving
        if(timeout_ms!=0) mk_port_delay_ms(1);
        else return MK_ERR_TIMEOUT;
    }
}
mk_status_t mk_mutex_unlock(mk_mutex_t* mutex){
    if(!mutex) return MK_ERR_INVALID;
    mk_port_enter_critical();
    if(!mutex->locked){ mk_port_exit_critical(); return MK_ERR_INVALID; }
    mutex->locked=0;
    mutex->owner=NULL;
    mk_port_exit_critical();
    mk_port_yield();
    return MK_OK;
}
bool mk_mutex_is_locked(mk_mutex_t* mutex){ return mutex? mutex->locked!=0 : false; }
#else
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
#endif


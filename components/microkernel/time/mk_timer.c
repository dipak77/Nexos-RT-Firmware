#include "mk_timer.h"
#include "mk_chip_port.h"
#include <stdlib.h>
struct mk_timer { TimerHandle_t handle; mk_timer_callback_t cb; void* arg; };
static void timer_cb_wrapper(TimerHandle_t xTimer){
    mk_timer_t* t = (mk_timer_t*)pvTimerGetTimerID(xTimer);
    if(t && t->cb) t->cb(t->arg);
}
mk_timer_t* mk_timer_create(const char* name, uint32_t period_ms, bool auto_reload, mk_timer_callback_t cb, void* arg){
    mk_timer_t* t = (mk_timer_t*)malloc(sizeof(mk_timer_t));
    if(!t) return NULL;
    t->cb = cb; t->arg = arg;
    t->handle = xTimerCreate(name ? name : "mk_timer", pdMS_TO_TICKS(period_ms), auto_reload?pdTRUE:pdFALSE, t, timer_cb_wrapper);
    if(!t->handle){ free(t); return NULL; }
    return t;
}
mk_status_t mk_timer_delete(mk_timer_t* timer){
    if(!timer) return MK_ERR_INVALID;
    timer->cb = NULL;
    timer->arg = NULL;
    xTimerStop(timer->handle, pdMS_TO_TICKS(100));
    xTimerDelete(timer->handle, pdMS_TO_TICKS(100));
    free(timer);
    return MK_OK;
}
mk_status_t mk_timer_start(mk_timer_t* timer, uint32_t timeout_ms){ if(!timer) return MK_ERR_INVALID; return xTimerStart(timer->handle, pdMS_TO_TICKS(timeout_ms))==pdTRUE?MK_OK:MK_ERR_INVALID; }
mk_status_t mk_timer_stop(mk_timer_t* timer, uint32_t timeout_ms){ if(!timer) return MK_ERR_INVALID; return xTimerStop(timer->handle, pdMS_TO_TICKS(timeout_ms))==pdTRUE?MK_OK:MK_ERR_INVALID; }
mk_status_t mk_timer_reset(mk_timer_t* timer){ if(!timer) return MK_ERR_INVALID; return xTimerReset(timer->handle, 0)==pdTRUE?MK_OK:MK_ERR_INVALID; }
bool mk_timer_is_active(mk_timer_t* timer){ if(!timer) return false; return xTimerIsTimerActive(timer->handle)==pdTRUE; }


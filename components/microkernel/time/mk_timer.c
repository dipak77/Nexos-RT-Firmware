#include "mk_timer.h"
#include "mk_chip_port.h"
#include "mk_config.h"
#include <stdlib.h>
#include "esp_timer.h"
#include "esp_heap_caps.h"

#if MK_NATIVE_KERNEL
// Native timer — esp_timer (no FreeRTOS Timer)
struct mk_timer { esp_timer_handle_t handle; mk_timer_callback_t cb; void* arg; bool auto_reload; uint32_t period_ms; };
static void native_timer_cb(void* arg){
    mk_timer_t* t=(mk_timer_t*)arg;
    if(t && t->cb) t->cb(t->arg);
}
mk_timer_t* mk_timer_create(const char* name, uint32_t period_ms, bool auto_reload, mk_timer_callback_t cb, void* arg){
    mk_timer_t* t=heap_caps_malloc(sizeof(*t), MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
    if(!t) t=malloc(sizeof(*t));
    if(!t) return NULL;
    t->cb=cb; t->arg=arg; t->auto_reload=auto_reload; t->period_ms=period_ms;
    esp_timer_create_args_t args={ .callback=native_timer_cb, .arg=t, .name=name?name:"mk_timer", .dispatch_method=ESP_TIMER_TASK };
    if(esp_timer_create(&args, &t->handle)!=ESP_OK){ heap_caps_free(t); return NULL; }
    return t;
}
mk_status_t mk_timer_delete(mk_timer_t* timer){
    if(!timer) return MK_ERR_INVALID;
    esp_timer_stop(timer->handle); esp_timer_delete(timer->handle); heap_caps_free(timer); return MK_OK;
}
mk_status_t mk_timer_start(mk_timer_t* timer, uint32_t timeout_ms){ (void)timeout_ms; if(!timer) return MK_ERR_INVALID; uint64_t us=(uint64_t)timer->period_ms*1000; esp_err_t r = timer->auto_reload? esp_timer_start_periodic(timer->handle, us) : esp_timer_start_once(timer->handle, us); return r==ESP_OK?MK_OK:MK_ERR_INVALID; }
mk_status_t mk_timer_stop(mk_timer_t* timer, uint32_t timeout_ms){ (void)timeout_ms; if(!timer) return MK_ERR_INVALID; esp_timer_stop(timer->handle); return MK_OK; }
mk_status_t mk_timer_reset(mk_timer_t* timer){ if(!timer) return MK_ERR_INVALID; esp_timer_stop(timer->handle); uint64_t us=(uint64_t)timer->period_ms*1000; esp_err_t r = timer->auto_reload? esp_timer_start_periodic(timer->handle, us) : esp_timer_start_once(timer->handle, us); return r==ESP_OK?MK_OK:MK_ERR_INVALID; }
bool mk_timer_is_active(mk_timer_t* timer){ if(!timer) return false; return esp_timer_is_active(timer->handle); }
#else
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
#endif


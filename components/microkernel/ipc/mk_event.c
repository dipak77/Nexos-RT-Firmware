#include "mk_event.h"
#include "mk_chip_port.h"
#include "mk_config.h"
#include <stdlib.h>
#include <string.h>
#include "mk_port.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"

#if MK_NATIVE_KERNEL
struct mk_event_group {
    uint32_t bits;
    portMUX_TYPE lock;
};

mk_event_group_t* mk_event_group_create(const char* name){
    (void)name;
    mk_event_group_t* g = (mk_event_group_t*)heap_caps_malloc(sizeof(*g), MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
    if(!g) g = (mk_event_group_t*)malloc(sizeof(*g));
    g->bits = 0;
    memset((void*)&g->lock, 0, sizeof(g->lock));
    portMUX_INITIALIZE(&g->lock);
    return g;
}

mk_status_t mk_event_group_delete(mk_event_group_t* group){
    if(!group) return MK_ERR_INVALID;
    heap_caps_free(group);
    return MK_OK;
}

mk_event_bits_t mk_event_set(mk_event_group_t* group, mk_event_bits_t bits){
    if(!group) return 0;
    taskENTER_CRITICAL(&group->lock);
    group->bits |= bits;
    uint32_t r = group->bits;
    taskEXIT_CRITICAL(&group->lock);
    return r;
}

mk_event_bits_t mk_event_clear(mk_event_group_t* group, mk_event_bits_t bits){
    if(!group) return 0;
    taskENTER_CRITICAL(&group->lock);
    group->bits &= ~bits;
    uint32_t r = group->bits;
    taskEXIT_CRITICAL(&group->lock);
    return r;
}

mk_event_bits_t mk_event_wait(mk_event_group_t* group, mk_event_bits_t bits_to_wait, bool clear_on_exit, bool wait_all, uint32_t timeout_ms){
    if(!group) return 0;
    uint64_t until = (timeout_ms == 0xFFFFFFFF) ? UINT64_MAX : esp_timer_get_time() + (uint64_t)timeout_ms * 1000;
    while(1){
        taskENTER_CRITICAL(&group->lock);
        uint32_t cur = group->bits;
        bool ok = wait_all ? ((cur & bits_to_wait) == bits_to_wait) : ((cur & bits_to_wait) != 0);
        if(ok){
            uint32_t ret = cur & bits_to_wait;
            if(clear_on_exit) group->bits &= ~bits_to_wait;
            taskEXIT_CRITICAL(&group->lock);
            return ret;
        }
        taskEXIT_CRITICAL(&group->lock);
        if(timeout_ms == 0 || esp_timer_get_time() >= until) {
            return cur & bits_to_wait;
        }
        mk_port_delay_ms(1);
    }
}

mk_event_bits_t mk_event_get(mk_event_group_t* group){
    if(!group) return 0;
    taskENTER_CRITICAL(&group->lock);
    uint32_t b = group->bits;
    taskEXIT_CRITICAL(&group->lock);
    return b;
}

#else
struct mk_event_group { EventGroupHandle_t handle; };
mk_event_group_t* mk_event_group_create(const char* name){
    mk_event_group_t* g = (mk_event_group_t*)malloc(sizeof(mk_event_group_t));
    if(!g) return NULL;
    g->handle = xEventGroupCreate();
    if(!g->handle){ free(g); return NULL; }
    return g;
}
mk_status_t mk_event_group_delete(mk_event_group_t* group){ if(!group) return MK_ERR_INVALID; vEventGroupDelete(group->handle); free(group); return MK_OK; }
mk_event_bits_t mk_event_set(mk_event_group_t* group, mk_event_bits_t bits){ if(!group) return 0; return xEventGroupSetBits(group->handle, bits); }
mk_event_bits_t mk_event_clear(mk_event_group_t* group, mk_event_bits_t bits){ if(!group) return 0; return xEventGroupClearBits(group->handle, bits); }
mk_event_bits_t mk_event_wait(mk_event_group_t* group, mk_event_bits_t bits_to_wait, bool clear_on_exit, bool wait_all, uint32_t timeout_ms){
    if(!group) return 0;
    TickType_t ticks = timeout_ms==0xFFFFFFFF ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xEventGroupWaitBits(group->handle, bits_to_wait, clear_on_exit?pdTRUE:pdFALSE, wait_all?pdTRUE:pdFALSE, ticks);
}
mk_event_bits_t mk_event_get(mk_event_group_t* group){ if(!group) return 0; return xEventGroupGetBits(group->handle); }
#endif

#include "mk_event.h"
#include "mk_chip_port.h"
#include <stdlib.h>
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

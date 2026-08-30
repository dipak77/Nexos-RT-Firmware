#include "mk_queue.h"
#include "mk_chip_port.h"
#include <stdlib.h>
struct mk_queue { QueueHandle_t handle; size_t item_size; };
mk_queue_t* mk_queue_create(size_t length, size_t item_size, const char* name){
    mk_queue_t* q = (mk_queue_t*)malloc(sizeof(mk_queue_t));
    if(!q) return NULL;
    q->handle = xQueueCreate(length, item_size);
    if(!q->handle){ free(q); return NULL; }
    q->item_size = item_size;
    return q;
}
mk_status_t mk_queue_delete(mk_queue_t* queue){ if(!queue) return MK_ERR_INVALID; vQueueDelete(queue->handle); free(queue); return MK_OK; }
mk_status_t mk_queue_send(mk_queue_t* queue, const void* item, uint32_t timeout_ms){
    if(!queue) return MK_ERR_INVALID;
    TickType_t ticks = timeout_ms==0xFFFFFFFF ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xQueueSend(queue->handle, item, ticks)==pdTRUE ? MK_OK : MK_ERR_TIMEOUT;
}
mk_status_t mk_queue_receive(mk_queue_t* queue, void* item, uint32_t timeout_ms){
    if(!queue) return MK_ERR_INVALID;
    TickType_t ticks = timeout_ms==0xFFFFFFFF ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xQueueReceive(queue->handle, item, ticks)==pdTRUE ? MK_OK : MK_ERR_TIMEOUT;
}
mk_status_t mk_queue_send_isr(mk_queue_t* queue, const void* item){
    if(!queue) return MK_ERR_INVALID;
    BaseType_t woken=pdFALSE;
    if(xQueueSendFromISR(queue->handle, item, &woken)==pdTRUE) return MK_OK;
    return MK_ERR_BUSY;
}
mk_status_t mk_queue_peek(mk_queue_t* queue, void* item, uint32_t timeout_ms){
    if(!queue) return MK_ERR_INVALID;
    TickType_t ticks = pdMS_TO_TICKS(timeout_ms);
    return xQueuePeek(queue->handle, item, ticks)==pdTRUE ? MK_OK : MK_ERR_TIMEOUT;
}
size_t mk_queue_get_count(mk_queue_t* queue){ if(!queue) return 0; return uxQueueMessagesWaiting(queue->handle); }
bool mk_queue_is_full(mk_queue_t* queue){ if(!queue) return true; return uxQueueSpacesAvailable(queue->handle)==0; }

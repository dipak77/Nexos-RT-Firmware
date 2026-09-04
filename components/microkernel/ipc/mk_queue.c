#include "mk_queue.h"
#include "mk_chip_port.h"
#include "mk_config.h"
#include <stdlib.h>
#include <string.h>
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "mk_port.h"

#if MK_NATIVE_KERNEL
// Native bounded circular buffer queue with atomic metrics
struct mk_queue {
    uint8_t* buf;
    size_t cap;
    size_t item_size;
    volatile size_t head;
    volatile size_t tail;
    volatile size_t count;
    portMUX_TYPE lock;
};

mk_queue_t* mk_queue_create(size_t length, size_t item_size, const char* name){
    (void)name;
    if(length == 0 || item_size == 0) return NULL;
    mk_queue_t* q = (mk_queue_t*)heap_caps_malloc(sizeof(*q), MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
    if(!q) q = (mk_queue_t*)malloc(sizeof(*q));
    if(!q) return NULL;
    q->buf = (uint8_t*)heap_caps_malloc(length * item_size, MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
    if(!q->buf){
        heap_caps_free(q);
        return NULL;
    }
    q->cap = length;
    q->item_size = item_size;
    q->head = 0;
    q->tail = 0;
    q->count = 0;
    memset((void*)&q->lock, 0, sizeof(q->lock));
    portMUX_INITIALIZE(&q->lock);
    return q;
}

mk_status_t mk_queue_delete(mk_queue_t* queue){
    if(!queue) return MK_ERR_INVALID;
    if(queue->buf) heap_caps_free(queue->buf);
    heap_caps_free(queue);
    return MK_OK;
}

mk_status_t mk_queue_send(mk_queue_t* queue, const void* item, uint32_t timeout_ms){
    if(!queue || !item) return MK_ERR_INVALID;
    uint64_t until = (timeout_ms == 0xFFFFFFFF) ? UINT64_MAX : esp_timer_get_time() + (uint64_t)timeout_ms * 1000;
    while(1){
        taskENTER_CRITICAL(&queue->lock);
        if(queue->count < queue->cap){
            memcpy(queue->buf + queue->tail * queue->item_size, item, queue->item_size);
            queue->tail = (queue->tail + 1) % queue->cap;
            queue->count++;
            taskEXIT_CRITICAL(&queue->lock);
            return MK_OK;
        }
        taskEXIT_CRITICAL(&queue->lock);
        if(esp_timer_get_time() >= until) return MK_ERR_TIMEOUT;
        mk_port_yield();
        if(timeout_ms != 0) mk_port_delay_ms(1);
        else return MK_ERR_TIMEOUT;
    }
}

mk_status_t mk_queue_receive(mk_queue_t* queue, void* item, uint32_t timeout_ms){
    if(!queue || !item) return MK_ERR_INVALID;
    uint64_t until = (timeout_ms == 0xFFFFFFFF) ? UINT64_MAX : esp_timer_get_time() + (uint64_t)timeout_ms * 1000;
    while(1){
        taskENTER_CRITICAL(&queue->lock);
        if(queue->count > 0){
            memcpy(item, queue->buf + queue->head * queue->item_size, queue->item_size);
            queue->head = (queue->head + 1) % queue->cap;
            queue->count--;
            taskEXIT_CRITICAL(&queue->lock);
            return MK_OK;
        }
        taskEXIT_CRITICAL(&queue->lock);
        if(esp_timer_get_time() >= until) return MK_ERR_TIMEOUT;
        mk_port_yield();
        if(timeout_ms != 0) mk_port_delay_ms(1);
        else return MK_ERR_TIMEOUT;
    }
}

mk_status_t mk_queue_send_isr(mk_queue_t* queue, const void* item){
    // Non-blocking ISR safe path
    if(!queue || !item) return MK_ERR_INVALID;
    bool ok = false;
    taskENTER_CRITICAL(&queue->lock);
    if(queue->count < queue->cap){
        memcpy(queue->buf + queue->tail * queue->item_size, item, queue->item_size);
        queue->tail = (queue->tail + 1) % queue->cap;
        queue->count++;
        ok = true;
    }
    taskEXIT_CRITICAL(&queue->lock);
    return ok ? MK_OK : MK_ERR_BUSY;
}

mk_status_t mk_queue_peek(mk_queue_t* queue, void* item, uint32_t timeout_ms){
    if(!queue || !item) return MK_ERR_INVALID;
    uint64_t until = (timeout_ms == 0xFFFFFFFF) ? UINT64_MAX : esp_timer_get_time() + (uint64_t)timeout_ms * 1000;
    while(1){
        taskENTER_CRITICAL(&queue->lock);
        if(queue->count > 0){
            memcpy(item, queue->buf + queue->head * queue->item_size, queue->item_size);
            taskEXIT_CRITICAL(&queue->lock);
            return MK_OK;
        }
        taskEXIT_CRITICAL(&queue->lock);
        if(esp_timer_get_time() >= until) return MK_ERR_TIMEOUT;
        mk_port_yield();
        if(timeout_ms != 0) mk_port_delay_ms(1);
        else return MK_ERR_TIMEOUT;
    }
}

size_t mk_queue_get_count(mk_queue_t* queue){
    if(!queue) return 0;
    taskENTER_CRITICAL(&queue->lock);
    size_t c = queue->count;
    taskEXIT_CRITICAL(&queue->lock);
    return c;
}

bool mk_queue_is_full(mk_queue_t* queue){
    if(!queue) return true;
    taskENTER_CRITICAL(&queue->lock);
    bool full = (queue->count >= queue->cap);
    taskEXIT_CRITICAL(&queue->lock);
    return full;
}

#else
// Legacy FreeRTOS shim
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
#endif

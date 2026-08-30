#pragma once
#include "mk_types.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mk_queue mk_queue_t;
mk_queue_t* mk_queue_create(size_t length, size_t item_size, const char* name);
mk_status_t mk_queue_delete(mk_queue_t* queue);
mk_status_t mk_queue_send(mk_queue_t* queue, const void* item, uint32_t timeout_ms);
mk_status_t mk_queue_receive(mk_queue_t* queue, void* item, uint32_t timeout_ms);
mk_status_t mk_queue_send_isr(mk_queue_t* queue, const void* item);
mk_status_t mk_queue_peek(mk_queue_t* queue, void* item, uint32_t timeout_ms);
size_t mk_queue_get_count(mk_queue_t* queue);
bool mk_queue_is_full(mk_queue_t* queue);
#ifdef __cplusplus
}
#endif

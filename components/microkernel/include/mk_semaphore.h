#pragma once
#include "mk_types.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mk_semaphore mk_semaphore_t;
mk_semaphore_t* mk_semaphore_create(uint32_t max_count, uint32_t initial_count, const char* name);
mk_status_t mk_semaphore_delete(mk_semaphore_t* sem);
mk_status_t mk_semaphore_take(mk_semaphore_t* sem, uint32_t timeout_ms);
mk_status_t mk_semaphore_give(mk_semaphore_t* sem);
uint32_t mk_semaphore_get_count(mk_semaphore_t* sem);
#ifdef __cplusplus
}
#endif

#pragma once
#include "mk_types.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct mk_mutex mk_mutex_t;

mk_mutex_t* mk_mutex_create(const char* name);
mk_status_t mk_mutex_delete(mk_mutex_t* mutex);
mk_status_t mk_mutex_lock(mk_mutex_t* mutex, uint32_t timeout_ms);
mk_status_t mk_mutex_try_lock(mk_mutex_t* mutex);
mk_status_t mk_mutex_unlock(mk_mutex_t* mutex);
bool mk_mutex_is_locked(mk_mutex_t* mutex);
const char* mk_mutex_get_name(mk_mutex_t* mutex);
void* mk_mutex_get_owner(mk_mutex_t* mutex);

#ifdef __cplusplus
}
#endif

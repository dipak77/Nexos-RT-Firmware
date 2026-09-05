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
// Recovery acquisition always returns MK_OK; query this BEFORE unlock to learn
// whether the previous owner died (protected state may need re-validation).
bool mk_mutex_was_owner_dead(mk_mutex_t* mutex);
const char* mk_mutex_get_name(mk_mutex_t* mutex);
void* mk_mutex_get_owner(mk_mutex_t* mutex);
mk_status_t mk_mutex_mark_owner_dead(mk_mutex_t* mutex);
void mk_mutex_reclaim_for_task(mk_task_t* task);

#ifdef __cplusplus
}
#endif

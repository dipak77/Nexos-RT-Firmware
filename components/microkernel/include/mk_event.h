#pragma once
#include "mk_types.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mk_event_group mk_event_group_t;
typedef uint32_t mk_event_bits_t;

mk_event_group_t* mk_event_group_create(const char* name);
mk_status_t mk_event_group_delete(mk_event_group_t* group);
mk_event_bits_t mk_event_set(mk_event_group_t* group, mk_event_bits_t bits);
mk_event_bits_t mk_event_clear(mk_event_group_t* group, mk_event_bits_t bits);
mk_event_bits_t mk_event_wait(mk_event_group_t* group, mk_event_bits_t bits_to_wait, bool clear_on_exit, bool wait_all, uint32_t timeout_ms);
mk_event_bits_t mk_event_get(mk_event_group_t* group);
#ifdef __cplusplus
}
#endif

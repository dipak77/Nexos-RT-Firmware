#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "mk.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Nexos-RT POSIX Compatibility Layer (Claim 4 Lite) */

typedef void* nexos_pthread_t;

typedef struct {
    size_t stack_size;
    int priority;
} nexos_pthread_attr_t;

typedef struct {
    mk_mutex_t* native_mutex;
} nexos_pthread_mutex_t;

#define NEXOS_PTHREAD_MUTEX_INITIALIZER { NULL }

int nexos_pthread_create(nexos_pthread_t* thread, const nexos_pthread_attr_t* attr,
                         void* (*start_routine)(void*), void* arg);
int nexos_pthread_join(nexos_pthread_t thread, void** retval);
int nexos_pthread_mutex_init(nexos_pthread_mutex_t* mutex, const void* attr);
int nexos_pthread_mutex_lock(nexos_pthread_mutex_t* mutex);
int nexos_pthread_mutex_trylock(nexos_pthread_mutex_t* mutex);
int nexos_pthread_mutex_unlock(nexos_pthread_mutex_t* mutex);
int nexos_pthread_mutex_destroy(nexos_pthread_mutex_t* mutex);

int nexos_sleep(unsigned int seconds);
int nexos_msleep(unsigned int msec);
int nexos_usleep(unsigned long usec);

/* Minimal POSIX Message Queue subset */
typedef struct {
    mk_queue_t* native_queue;
    size_t msg_size;
} nexos_mq_desc_t;

typedef nexos_mq_desc_t* nexos_mqd_t;

nexos_mqd_t nexos_mq_open(const char* name, int oflag, size_t max_msgs, size_t msg_size);
int nexos_mq_send(nexos_mqd_t mqdes, const char* msg_ptr, size_t msg_len, unsigned int msg_prio);
int nexos_mq_receive(nexos_mqd_t mqdes, char* msg_ptr, size_t msg_len, unsigned int* msg_prio);
int nexos_mq_close(nexos_mqd_t mqdes);

#ifdef __cplusplus
}
#endif

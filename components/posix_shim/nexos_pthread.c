#include "nexos_posix.h"
#include "esp_timer.h"
#include <stdlib.h>
#include <errno.h>

int nexos_pthread_create(nexos_pthread_t* thread, const nexos_pthread_attr_t* attr,
                         void* (*start_routine)(void*), void* arg) {
    if (!thread || !start_routine) return EINVAL;

    size_t stack = attr ? attr->stack_size : 4096;
    uint8_t prio = attr ? (uint8_t)attr->priority : MK_PRIO_COMMAND;

    mk_task_handle_t t = mk_task_create("posix_th", (mk_task_entry_t)start_routine, arg, NULL, stack, prio);
    if (!t) return ENOMEM;

    *thread = (nexos_pthread_t)t;
    return 0;
}

int nexos_pthread_join(nexos_pthread_t thread, void** retval) {
    (void)thread;
    (void)retval;
    // Basic detachment/yield wait
    mk_sleep_ms(10);
    return 0;
}

int nexos_pthread_mutex_init(nexos_pthread_mutex_t* mutex, const void* attr) {
    (void)attr;
    if (!mutex) return EINVAL;
    mutex->native_mutex = mk_mutex_create("posix_mux");
    return mutex->native_mutex ? 0 : ENOMEM;
}

int nexos_pthread_mutex_lock(nexos_pthread_mutex_t* mutex) {
    if (!mutex) return EINVAL;
    if (!mutex->native_mutex) {
        int r = nexos_pthread_mutex_init(mutex, NULL);
        if (r != 0) return r;
    }
    mk_status_t st = mk_mutex_lock(mutex->native_mutex, 0xFFFFFFFF);
    if (st == MK_OK) return 0;
    if (st == MK_ERR_DEADLOCK_OWNER_DEAD) return EOWNERDEAD;
    return EBUSY;
}

int nexos_pthread_mutex_trylock(nexos_pthread_mutex_t* mutex) {
    if (!mutex) return EINVAL;
    if (!mutex->native_mutex) {
        int r = nexos_pthread_mutex_init(mutex, NULL);
        if (r != 0) return r;
    }
    mk_status_t st = mk_mutex_try_lock(mutex->native_mutex);
    return (st == MK_OK) ? 0 : EBUSY;
}

int nexos_pthread_mutex_unlock(nexos_pthread_mutex_t* mutex) {
    if (!mutex || !mutex->native_mutex) return EINVAL;
    mk_status_t st = mk_mutex_unlock(mutex->native_mutex);
    return (st == MK_OK) ? 0 : EPERM;
}

int nexos_pthread_mutex_destroy(nexos_pthread_mutex_t* mutex) {
    if (!mutex) return EINVAL;
    if (mutex->native_mutex) {
        mk_mutex_delete(mutex->native_mutex);
        mutex->native_mutex = NULL;
    }
    return 0;
}

int nexos_sleep(unsigned int seconds) {
    mk_sleep_ms((uint32_t)seconds * 1000);
    return 0;
}

int nexos_msleep(unsigned int msec) {
    mk_sleep_ms(msec);
    return 0;
}

int nexos_usleep(unsigned long usec) {
    uint32_t ms = (uint32_t)(usec / 1000);
    if (ms == 0) ms = 1;
    mk_sleep_ms(ms);
    return 0;
}

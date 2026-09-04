#include "nexos_posix.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>

nexos_mqd_t nexos_mq_open(const char* name, int oflag, size_t max_msgs, size_t msg_size) {
    (void)oflag;
    if (max_msgs == 0 || msg_size == 0) return (nexos_mqd_t)-1;

    nexos_mq_desc_t* desc = (nexos_mq_desc_t*)malloc(sizeof(nexos_mq_desc_t));
    if (!desc) return (nexos_mqd_t)-1;

    desc->native_queue = mk_queue_create(max_msgs, msg_size, name);
    if (!desc->native_queue) {
        free(desc);
        return (nexos_mqd_t)-1;
    }
    desc->msg_size = msg_size;
    return desc;
}

int nexos_mq_send(nexos_mqd_t mqdes, const char* msg_ptr, size_t msg_len, unsigned int msg_prio) {
    (void)msg_prio;
    if (!mqdes || mqdes == (nexos_mqd_t)-1 || !msg_ptr || msg_len > mqdes->msg_size) {
        return EINVAL;
    }
    mk_status_t st = mk_queue_send(mqdes->native_queue, msg_ptr, 0xFFFFFFFF);
    return (st == MK_OK) ? 0 : EAGAIN;
}

int nexos_mq_receive(nexos_mqd_t mqdes, char* msg_ptr, size_t msg_len, unsigned int* msg_prio) {
    (void)msg_prio;
    if (!mqdes || mqdes == (nexos_mqd_t)-1 || !msg_ptr || msg_len < mqdes->msg_size) {
        return EINVAL;
    }
    mk_status_t st = mk_queue_receive(mqdes->native_queue, msg_ptr, 0xFFFFFFFF);
    return (st == MK_OK) ? (int)mqdes->msg_size : -1;
}

int nexos_mq_close(nexos_mqd_t mqdes) {
    if (!mqdes || mqdes == (nexos_mqd_t)-1) return EINVAL;
    if (mqdes->native_queue) {
        mk_queue_delete(mqdes->native_queue);
    }
    free(mqdes);
    return 0;
}

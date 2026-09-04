#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MK_IPI_YIELD = 0,
    MK_IPI_NET_PACKET_READY,
    MK_IPI_RADIO_STATUS_CHANGE,
    MK_IPI_SHUTDOWN_REQUEST,
} mk_ipi_msg_t;

typedef void (*mk_ipi_handler_t)(mk_ipi_msg_t msg, void* arg);

/**
 * @brief Initialize cross-core IPI communication between Core 0 and Core 1.
 */
bool mk_ipi_init(void);

/**
 * @brief Send an IPI message to target core.
 * @param target_core 0 or 1
 * @param msg Message type
 */
void mk_ipi_send(int target_core, mk_ipi_msg_t msg);

/**
 * @brief Register handler for received IPI messages on the calling core.
 */
void mk_ipi_register_handler(mk_ipi_msg_t msg, mk_ipi_handler_t handler, void* arg);

#ifdef __cplusplus
}
#endif

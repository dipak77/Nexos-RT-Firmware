#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "mk_types.h"
#include "mk_enclave.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MK_SVC_YIELD = 1,
    MK_SVC_SLEEP_MS = 2,
    MK_SVC_GET_TIME_US = 3,
    MK_SVC_GFX_FLUSH = 10,
    MK_SVC_GFX_ACQUIRE = 11,
    MK_SVC_GFX_RELEASE = 12,
    MK_SVC_NET_SEND = 20,
    MK_SVC_NET_RECV = 21,
    MK_SVC_GPIO_SET = 30,
    MK_SVC_GPIO_GET = 31,
    MK_SVC_IPC_SEND = 40,
    MK_SVC_IPC_RECV = 41,
} mk_svc_num_t;

typedef struct {
    uintptr_t arg0;
    uintptr_t arg1;
    uintptr_t arg2;
    uintptr_t arg3;
} mk_svc_args_t;

/**
 * @brief Dispatch a system call through the validated Software SVC Gate.
 * Enforces:
 *  1. Capability bitmask check against current enclave
 *  2. Pointer range verification within [base, limit)
 *  3. Entry point allowlist
 *  4. Strict <50us execution budget without heap allocation
 */
mk_status_t mk_svc_dispatch(mk_svc_num_t svc_num, mk_svc_args_t* args, uintptr_t* out_result);

/**
 * @brief Helper to validate that a memory buffer is strictly inside caller enclave bounds.
 */
bool mk_svc_validate_ptr(const mk_enclave_desc_t* desc, const void* ptr, size_t len);

/**
 * @brief Register driver entry point in the kernel allowlist.
 */
mk_status_t mk_svc_register_driver(mk_svc_num_t svc_num, uint32_t required_cap, void* handler);

#ifdef __cplusplus
}
#endif

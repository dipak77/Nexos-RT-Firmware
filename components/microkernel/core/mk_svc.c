#include "mk_svc.h"
#include "mk_fault.h"
#include "mk_port.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>

static const char* TAG = "MK_SVC";

typedef struct {
    uint32_t required_cap;
    void* handler;
} svc_entry_t;

#define MK_MAX_SVC_NUM 64
static svc_entry_t s_svc_table[MK_MAX_SVC_NUM];
static bool s_svc_initialized = false;

static void svc_table_init_defaults(void) {
    if (s_svc_initialized) return; // idempotent: never wipe registrations
    memset(s_svc_table, 0, sizeof(s_svc_table));
    // Core kernel services require no special capability
    s_svc_table[MK_SVC_YIELD].required_cap = MK_CAP_NONE;
    s_svc_table[MK_SVC_SLEEP_MS].required_cap = MK_CAP_NONE;
    s_svc_table[MK_SVC_GET_TIME_US].required_cap = MK_CAP_NONE;

    // GFX driver capability
    s_svc_table[MK_SVC_GFX_FLUSH].required_cap = MK_CAP_GFX;
    s_svc_table[MK_SVC_GFX_ACQUIRE].required_cap = MK_CAP_GFX;
    s_svc_table[MK_SVC_GFX_RELEASE].required_cap = MK_CAP_GFX;

    // Network driver capability
    s_svc_table[MK_SVC_NET_SEND].required_cap = MK_CAP_NET;
    s_svc_table[MK_SVC_NET_RECV].required_cap = MK_CAP_NET;

    // GPIO driver capability
    s_svc_table[MK_SVC_GPIO_SET].required_cap = MK_CAP_GPIO;
    s_svc_table[MK_SVC_GPIO_GET].required_cap = MK_CAP_GPIO;

    // IPC capability
    s_svc_table[MK_SVC_IPC_SEND].required_cap = MK_CAP_NONE;
    s_svc_table[MK_SVC_IPC_RECV].required_cap = MK_CAP_NONE;
    s_svc_initialized = true;
}

bool mk_svc_validate_ptr(const mk_enclave_desc_t* desc, const void* ptr, size_t len) {
    if (!desc || !ptr) return false;
    uintptr_t p = (uintptr_t)ptr;
    uintptr_t end = p + len;
    if (end < p) return false; // Overflow check

    // Check 1: Inside enclave heap/data window
    if (desc->base > 0 && desc->limit > desc->base) {
        if (p >= desc->base && end <= desc->limit) {
            return true;
        }
    }

    // Check 2: Inside enclave stack
    if (desc->stack_base > 0 && desc->stack_size > 0) {
        uintptr_t stack_limit = desc->stack_base + desc->stack_size;
        if (p >= desc->stack_base && end <= stack_limit) {
            return true;
        }
    }

    // Pointer is outside both data window and stack -> Out-Of-Bounds!
    return false;
}

mk_status_t mk_svc_register_driver(mk_svc_num_t svc_num, uint32_t required_cap, void* handler) {
    if ((int)svc_num < 0 || (int)svc_num >= MK_MAX_SVC_NUM) {
        return MK_ERR_INVALID;
    }
    svc_table_init_defaults(); // init-if-needed so early registration survives
    s_svc_table[svc_num].required_cap = required_cap;
    s_svc_table[svc_num].handler = handler;
    return MK_OK;
}

mk_status_t mk_svc_dispatch(mk_svc_num_t svc_num, mk_svc_args_t* args, uintptr_t* out_result) {
    svc_table_init_defaults();

    if ((int)svc_num < 0 || (int)svc_num >= MK_MAX_SVC_NUM) {
        return MK_ERR_INVALID;
    }

    mk_enclave_desc_t* enclave = mk_enclave_get_current();
    uint32_t required_cap = s_svc_table[svc_num].required_cap;

    // 1. Enforce Capability Validation (Claim 3). Fail CLOSED: a privileged
    // SVC with no bound enclave is denied. Only MK_CAP_NONE services (yield,
    // sleep, time, IPC) are callable without an enclave.
    if (required_cap != MK_CAP_NONE) {
        if (enclave == NULL) {
            ESP_LOGE(TAG, "[TRAP] Denied privileged syscall=%d with no enclave bound", (int)svc_num);
            return MK_ERR_CAPABILITY;
        }
        if ((enclave->syscall_mask & required_cap) != required_cap) {
            ESP_LOGE(TAG, "[TRAP] Capability violation in [%s]: syscall=%d requires cap=0x%08lX, mask=0x%08lX",
                     enclave->name, (int)svc_num, (unsigned long)required_cap, (unsigned long)enclave->syscall_mask);

            mk_enclave_trap(enclave, MK_FAULT_CAPABILITY_VIOLATION, (uint16_t)svc_num, "capability_violation");
            return MK_ERR_CAPABILITY;
        }
    }

    // 2. Validate Argument Envelope and Out-Result Pointer (A8)
    if (enclave != NULL) {
        if (args != NULL && !mk_svc_validate_ptr(enclave, args, sizeof(mk_svc_args_t))) {
            ESP_LOGE(TAG, "[TRAP] OOB args pointer in [%s]: args=%p", enclave->name, args);
            mk_enclave_trap(enclave, MK_FAULT_OOB_MEMORY_ACCESS, (uint16_t)svc_num, "oob_args_pointer");
            return MK_ERR_SVC_FAULT;
        }
        if (out_result != NULL && !mk_svc_validate_ptr(enclave, out_result, sizeof(uintptr_t))) {
            ESP_LOGE(TAG, "[TRAP] OOB out_result pointer in [%s]: out_result=%p", enclave->name, out_result);
            mk_enclave_trap(enclave, MK_FAULT_OOB_MEMORY_ACCESS, (uint16_t)svc_num, "oob_out_result");
            return MK_ERR_SVC_FAULT;
        }
    }

    // 3. Validate Buffer Pointers for Memory-Consuming Syscalls (A8)
    if (enclave != NULL && args != NULL) {
        const void* buf = NULL;
        size_t len = 0;
        if (svc_num == MK_SVC_GFX_FLUSH || svc_num == MK_SVC_NET_SEND || svc_num == MK_SVC_NET_RECV) {
            buf = (const void*)args->arg0;
            len = (size_t)args->arg1;
        } else if (svc_num == MK_SVC_IPC_SEND || svc_num == MK_SVC_IPC_RECV) {
            buf = (const void*)args->arg1;
            len = (size_t)args->arg2;
        }
        if (buf && len > 0) {
            if (!mk_svc_validate_ptr(enclave, buf, len)) {
                ESP_LOGE(TAG, "[TRAP] OOB pointer in [%s]: buf=%p len=%u outside bounds [0x%lX, 0x%lX)",
                         enclave->name, buf, (unsigned)len,
                         (unsigned long)enclave->base, (unsigned long)enclave->limit);

                mk_enclave_trap(enclave, MK_FAULT_OOB_MEMORY_ACCESS, (uint16_t)svc_num, "oob_memory_access");
                return MK_ERR_SVC_FAULT;
            }
        }
    }

    // 3. Dispatch to Hardware Driver / Microkernel Function
    uintptr_t res = 0;
    switch (svc_num) {
        case MK_SVC_YIELD:
            mk_port_yield();
            break;
        case MK_SVC_SLEEP_MS:
            if (args) mk_port_delay_ms((uint32_t)args->arg0);
            break;
        case MK_SVC_GET_TIME_US:
            res = (uintptr_t)esp_timer_get_time();
            break;
        default:
            if (s_svc_table[svc_num].handler) {
                typedef uintptr_t (*svc_fn_t)(uintptr_t, uintptr_t, uintptr_t, uintptr_t);
                svc_fn_t fn = (svc_fn_t)s_svc_table[svc_num].handler;
                if (args) {
                    res = fn(args->arg0, args->arg1, args->arg2, args->arg3);
                } else {
                    res = fn(0, 0, 0, 0);
                }
            } else {
                // No driver registered: honest NOT_FOUND, never silent success.
                return MK_ERR_NOT_FOUND;
            }
            break;
    }

    if (out_result) {
        *out_result = res;
    }
    return MK_OK;
}

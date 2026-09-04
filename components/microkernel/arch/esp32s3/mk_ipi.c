#include "mk_ipi.h"
#include "mk_port.h"
#include "esp_ipc.h"
#include "esp_log.h"
#include <string.h>

static const char* TAG = "MK_IPI";

typedef struct {
    mk_ipi_handler_t handler;
    void* arg;
} ipi_callback_entry_t;

static ipi_callback_entry_t s_handlers[4] = {0};

static void ipi_remote_call(void* arg) {
    mk_ipi_msg_t msg = (mk_ipi_msg_t)(uintptr_t)arg;
    if ((int)msg >= 0 && (int)msg < 4) {
        if (s_handlers[msg].handler) {
            s_handlers[msg].handler(msg, s_handlers[msg].arg);
        }
    }
}

bool mk_ipi_init(void) {
    memset(s_handlers, 0, sizeof(s_handlers));
    ESP_LOGI(TAG, "Cross-core IPI communication initialized between Core 0 and Core 1");
    return true;
}

void mk_ipi_send(int target_core, mk_ipi_msg_t msg) {
    if (target_core < 0 || target_core > 1) return;
    int cur_core = mk_port_get_core_id();
    if (cur_core == target_core) {
        // Local execution
        ipi_remote_call((void*)(uintptr_t)msg);
        return;
    }

    // Dispatch asynchronous call to target core
    esp_ipc_call((uint32_t)target_core, ipi_remote_call, (void*)(uintptr_t)msg);
}

void mk_ipi_register_handler(mk_ipi_msg_t msg, mk_ipi_handler_t handler, void* arg) {
    if ((int)msg >= 0 && (int)msg < 4) {
        s_handlers[msg].handler = handler;
        s_handlers[msg].arg = arg;
    }
}

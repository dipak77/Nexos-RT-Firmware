#include "mk_ipi.h"
#include "mk_port.h"
#include "mk_chip_port.h"
#include "esp_ipc.h"
#include "esp_log.h"
#include <string.h>

static const char* TAG = "MK_IPI";

typedef struct {
    mk_ipi_handler_t handler;
    void* arg;
} ipi_callback_entry_t;

static ipi_callback_entry_t s_handlers[4] = {0};
static portMUX_TYPE s_ipi_lock = portMUX_INITIALIZER_UNLOCKED;

static void ipi_remote_call(void* arg) {
    mk_ipi_msg_t msg = (mk_ipi_msg_t)(uintptr_t)arg;
    mk_ipi_handler_t h = NULL;
    void* harg = NULL;
    // Copy under lock, call outside it: handler runtime must never hold the
    // table lock (it may register/unregister or send another IPI).
    taskENTER_CRITICAL(&s_ipi_lock);
    if ((int)msg >= 0 && (int)msg < 4) {
        h = s_handlers[msg].handler;
        harg = s_handlers[msg].arg;
    }
    taskEXIT_CRITICAL(&s_ipi_lock);
    if (h) h(msg, harg);
}

bool mk_ipi_init(void) {
    taskENTER_CRITICAL(&s_ipi_lock);
    memset(s_handlers, 0, sizeof(s_handlers));
    taskEXIT_CRITICAL(&s_ipi_lock);
    ESP_LOGI(TAG, "Cross-core IPI communication initialized between Core 0 and Core 1");
    return true;
}

void mk_ipi_send(int target_core, mk_ipi_msg_t msg) {
    // esp_ipc_call BLOCKS until the target core runs the callback: never call
    // from ISR or from inside a critical section (it would deadlock the target
    // core spinning on the caller's lock). No callers yet; documented for V2.5.
    if (target_core < 0 || target_core > 1) return;
    int cur_core = mk_port_get_core_id();
    if (cur_core == target_core) {
        // Local execution
        ipi_remote_call((void*)(uintptr_t)msg);
        return;
    }

    // Dispatch call to target core (blocking by ESP-IDF contract)
    esp_ipc_call((uint32_t)target_core, ipi_remote_call, (void*)(uintptr_t)msg);
}

void mk_ipi_register_handler(mk_ipi_msg_t msg, mk_ipi_handler_t handler, void* arg) {
    if ((int)msg >= 0 && (int)msg < 4) {
        taskENTER_CRITICAL(&s_ipi_lock);
        s_handlers[msg].handler = handler;
        s_handlers[msg].arg = arg;
        taskEXIT_CRITICAL(&s_ipi_lock);
    }
}

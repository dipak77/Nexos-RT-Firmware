#include "mk_kernel.h"
#include "mk_task.h"
#include "mk_scheduler.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "mk_port.h"

static const char* TAG = "NEXOS";
#define NEXOS_START_BIT 0x01

static bool s_initialized = false;
static bool s_running = false;
static mk_config_t s_config;
static mk_port_event_group_handle_t s_start_gate = NULL;

const char* mk_status_to_string(mk_status_t s){
    switch(s){
        case MK_OK: return "OK";
        case MK_ERR_TIMEOUT: return "TIMEOUT";
        case MK_ERR_NO_MEMORY: return "NO_MEMORY";
        case MK_ERR_INVALID: return "INVALID";
        case MK_ERR_NOT_INITIALIZED: return "NOT_INITIALIZED";
        case MK_ERR_BAD_STATE: return "BAD_STATE";
        default: return "ERROR";
    }
}
const char* mk_task_state_to_string(mk_task_state_t s){
    switch(s){
        case MK_TASK_READY: return "READY";
        case MK_TASK_RUNNING: return "RUN";
        case MK_TASK_BLOCKED: return "BLOCKED";
        case MK_TASK_SLEEPING: return "SLEEP";
        case MK_TASK_SUSPENDED: return "SUSPENDED";
        default: return "UNKNOWN";
    }
}

uint32_t mk_map_port_priority(uint8_t mk_prio) {
    // Distinct table: idle < time < storage < connectivity < command < diagnostics < gui <20 (below WiFi 22-23)
    uint32_t p = 8;
    switch (mk_prio) {
        case MK_PRIO_IDLE: p = 2; break;        // IDLE
        case MK_PRIO_STORAGE: p = 3; break;     // STORAGE 2→3
        case MK_PRIO_DIAGNOSTICS: p = 12; break;// DIAGNOSTICS 3→12
        case MK_PRIO_TIME: p = 5; break;        // TIME 4→5
        case MK_PRIO_CONNECTIVITY: p = 8; break;// CONNECTIVITY 5→8
        case MK_PRIO_COMMAND: p = 10; break;    // COMMAND 6→10 (distinct from TIME/CONNECTIVITY)
        case MK_PRIO_GUI: p = 16; break;        // GUI 7→16
        default: p = 8; break;
    }
    if (p > 20) p = 20;
    if (p > (uint32_t)(configMAX_PRIORITIES - 1)) p = (uint32_t)(configMAX_PRIORITIES - 1);
    return p;
}

mk_status_t mk_init(const mk_config_t* config){
    if(s_initialized) return MK_OK;
    if(config) s_config = *config;
    else {
        s_config.tick_hz = MK_CONFIG_TICK_HZ;
        s_config.use_preemption = true;
        s_config.single_core = false;
        s_config.version = MK_CONFIG_VERSION_STRING;
    }
    s_start_gate = mk_port_event_create("nexos_start");
    if (!s_start_gate) return MK_ERR_NO_MEMORY;
    mk_scheduler_init();
    s_initialized = true;
    s_running = false;
    ESP_LOGI(TAG, "%s v%s initialized tick=%luHz [%s]", MK_CONFIG_OS_NAME, s_config.version, (unsigned long)s_config.tick_hz, MK_NATIVE_KERNEL?"NATIVE":"FREERTOS_SHIM");
    return MK_OK;
}

mk_status_t mk_start(void){
    if(!s_initialized) return MK_ERR_NOT_INITIALIZED;
    if (s_running) return MK_ERR_BAD_STATE;
    s_running = true;
    mk_port_event_set(s_start_gate, NEXOS_START_BIT);
    ESP_LOGI(TAG, "%s started — %d tasks", MK_CONFIG_OS_NAME, mk_task_count());
    return MK_OK;
}

void mk_wait_start(void){
    if (!s_start_gate) return;
    mk_port_event_wait(s_start_gate, NEXOS_START_BIT, false, true, 0xFFFFFFFF);
}

int mk_current_core(void){
    return mk_port_get_core_id();
}

void mk_yield(void){
    mk_port_yield();
}

void mk_sleep_ms(uint32_t ms){
    mk_port_delay_ms(ms);
}

uint64_t mk_time_ms(void){
    return esp_timer_get_time()/1000;
}
uint64_t mk_time_us(void){
    return esp_timer_get_time();
}

bool mk_is_initialized(void){ return s_initialized; }
bool mk_is_running(void){ return s_running; }

mk_kernel_stats_t mk_kernel_get_stats(void){
    mk_kernel_stats_t stats = {0};
    stats.uptime_ms = mk_time_ms();
    stats.context_switches = mk_scheduler_get_context_switches();
    stats.total_tasks = mk_task_count();
    stats.free_heap = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    stats.min_free_heap = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
    stats.cpu_load_percent = mk_kernel_get_cpu_load();
    return stats;
}

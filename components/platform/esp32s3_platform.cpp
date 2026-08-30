#include "esp32s3_platform.h"
#include "esp_log.h"

static const char* TAG = "PLATFORM_ESP32S3";

namespace smart_device {
namespace platform {

Esp32S3Platform& Esp32S3Platform::instance(){ static Esp32S3Platform s; return s; }

bool Esp32S3Platform::init(){
    if(initialized_) return true;
    ESP_LOGI(TAG, "Platform init: core 0 chip drivers, core 1 %s application", MK_CONFIG_OS_NAME);
    initialized_=true;
    return true;
}

mk_task_handle_t Esp32S3Platform::create_pinned_task(const char* name, mk_task_entry_t entry, void* arg, size_t stack, uint8_t prio, int core){
    mk_task_config_t cfg{};
    cfg.name = name;
    cfg.priority = prio;
    cfg.stack_size = stack;
    cfg.core_affinity = core;
    return mk_task_create_ext(&cfg, entry, arg);
}

} // namespace platform
} // namespace smart_device

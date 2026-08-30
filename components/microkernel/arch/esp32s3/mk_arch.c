#include "mk_kernel.h"
#include "esp_log.h"
static const char* TAG = "MK_ARCH_ESP32S3";
void mk_arch_init(void){
    ESP_LOGI(TAG, "ESP32-S3 arch init — Xtensa LX7, Nexos-RT app pinned to core 1");
}

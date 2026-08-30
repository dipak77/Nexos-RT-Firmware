#include "mk_kernel.h"
#include "esp_log.h"
static const char* TAG = "NEXTOS_PORT";
void mk_port_shim_init(void){
    ESP_LOGI(TAG, "ESP32-S3 chip port active (Wi-Fi/BLE drivers on core 0, Nexos-RT app on core 1)");
}

#include "ota_service.h"
#include "mk.h"
#include "event_bus/event_bus.h"
#include "esp_log.h"
#include "esp_https_ota.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"

static const char* TAG = "OTA_SERVICE";

namespace smart_device {
namespace ota {

OtaService& OtaService::instance(){ static OtaService s; return s; }

Result<void> OtaService::initialize(){
    if(initialized_) return Result<void>::Ok();
    initialized_ = true;
    status_.state = OtaState::IDLE;
    ESP_LOGI(TAG, "OTA service initialized");
    return Result<void>::Ok();
}

Result<void> OtaService::check_and_update(const std::string& url, const std::string& cert_pem){
    if(is_in_progress()) return Result<void>::Err(AppError::OTA_IN_PROGRESS, "OTA already running");
    ESP_LOGI(TAG, "OTA starting URL=%s", url.c_str());

    status_.state = OtaState::DOWNLOADING;
    status_.progress_percent = 0;

    AppEvent ev{AppEventType::OTA_STARTED}; EventBus::instance().publish(ev);

    esp_http_client_config_t http_cfg{};
    http_cfg.url = url.c_str();
    http_cfg.timeout_ms = 10000;
    if(!cert_pem.empty()) http_cfg.cert_pem = cert_pem.c_str();
    http_cfg.skip_cert_common_name_check = cert_pem.empty();

    esp_https_ota_config_t ota_cfg{};
    ota_cfg.http_config = &http_cfg;

    esp_err_t ret = esp_https_ota(&ota_cfg);
    if(ret==ESP_OK){
        status_.state = OtaState::READY;
        status_.progress_percent = 100;
        ESP_LOGI(TAG, "OTA success, restarting...");
        AppEvent ev2{AppEventType::OTA_COMPLETE}; EventBus::instance().publish(ev2);
        mk_sleep_ms(1000);
        esp_restart();
        return Result<void>::Ok();
    } else {
        status_.state = OtaState::FAILED;
        snprintf(status_.error, sizeof(status_.error), "%s", esp_err_to_name(ret));
        ESP_LOGE(TAG, "OTA failed: %s", esp_err_to_name(ret));
        AppEvent ev3{AppEventType::OTA_FAILED, (uint32_t)ret, 0, ""}; snprintf(ev3.message, sizeof(ev3.message), "%s", esp_err_to_name(ret)); EventBus::instance().publish(ev3);
        return Result<void>::Err(AppError::OTA_DOWNLOAD_FAILED, esp_err_to_name(ret));
    }
}

Result<void> OtaService::rollback(){
    const esp_partition_t* running = esp_ota_get_running_partition();
    esp_ota_img_states_t state;
    if(esp_ota_get_state_partition(running, &state)==ESP_OK && state==ESP_OTA_IMG_PENDING_VERIFY){
        ESP_LOGI(TAG, "Rolling back");
        esp_ota_mark_app_invalid_rollback_and_reboot();
    }
    return Result<void>::Ok();
}

} // namespace ota
} // namespace smart_device

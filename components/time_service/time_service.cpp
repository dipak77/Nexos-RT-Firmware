#include "time_service.h"
#include "common/string_utils.h"
#include "event_bus/event_bus.h"
#include "storage/settings_store.h"
#include "esp_log.h"
#include "esp_sntp.h"
#include "esp_netif_sntp.h"
#include <cstring>

static const char* TAG = "TIME_SERVICE";

namespace smart_device {
namespace time_service {

TimeService& TimeService::instance(){ static TimeService s; return s; }

void TimeService::sntp_sync_cb(::timeval* tv){
    auto& inst = TimeService::instance();
    inst.status_.synced = true;
    inst.status_.source = TimeSource::SNTP;
    inst.status_.last_sync = tv->tv_sec;
    ESP_LOGI(TAG, "Time synced: %lld", (long long)tv->tv_sec);

    AppEvent ev{};
    ev.type = AppEventType::TIME_SYNCED;
    ev.code = 0;
    snprintf(ev.message, sizeof(ev.message), "SNTP synced");
    EventBus::instance().publish(ev);
}

Result<void> TimeService::initialize(const char* timezone){
    if(timezone) copy_cstr(status_.timezone, timezone);
    else copy_cstr(status_.timezone, "IST-5:30");

    setenv("TZ", status_.timezone, 1);
    tzset();

    // Try to restore last time from NVS? For now use system time
    time_t now; time(&now);
    if(now > 1700000000){ // valid timestamp after 2023
        status_.synced = false; // still need SNTP for accuracy
        status_.source = TimeSource::SYSTEM;
    }

    ESP_LOGI(TAG, "Time service initialized TZ=%s", status_.timezone);

    AppEvent ev{}; ev.type = AppEventType::TIME_SYNC_STARTED;
    EventBus::instance().publish(ev);
    return Result<void>::Ok();
}

Result<void> TimeService::start_sntp(){
    if(sntp_started_) return Result<void>::Ok();
    ESP_LOGI(TAG, "Starting SNTP");

    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    config.sync_cb = sntp_sync_cb;
    config.start = false;
    // India-friendly servers
    config.server_from_dhcp = false;
    // We'll set servers manually via sntp_setserver? esp_netif_sntp handles
    esp_err_t ret = esp_netif_sntp_init(&config);
    if(ret!=ESP_OK){
        ESP_LOGE(TAG, "SNTP init failed: %s", esp_err_to_name(ret));
        return Result<void>::Err(AppError::TIME_NTP_TIMEOUT, esp_err_to_name(ret));
    }
    ret = esp_netif_sntp_start();
    if(ret!=ESP_OK){
        ESP_LOGE(TAG, "SNTP start failed: %s", esp_err_to_name(ret));
        return Result<void>::Err(AppError::TIME_NTP_TIMEOUT, esp_err_to_name(ret));
    }
    sntp_started_ = true;
    return Result<void>::Ok();
}

Result<void> TimeService::stop_sntp(){
    if(!sntp_started_) return Result<void>::Ok();
    esp_netif_sntp_deinit();
    sntp_started_ = false;
    return Result<void>::Ok();
}

Result<void> TimeService::set_timezone(const char* tz){
    if(!tz) return Result<void>::Err(AppError::COMMAND_INVALID_PARAM, "null tz");
    copy_cstr(status_.timezone, tz);
    setenv("TZ", status_.timezone, 1);
    tzset();
    storage::SettingsStore::instance().settings().timezone[0]='\0';
    copy_cstr(storage::SettingsStore::instance().settings().timezone, tz);
    storage::SettingsStore::instance().save();
    ESP_LOGI(TAG, "Timezone set to %s", tz);
    return Result<void>::Ok();
}

Result<void> TimeService::set_time_manual(time_t t){
    ::timeval tv{.tv_sec = t, .tv_usec = 0};
    if(settimeofday(&tv, nullptr)!=0){
        return Result<void>::Err(AppError::TIME_INVALID, "settimeofday failed");
    }
    status_.synced = true;
    status_.source = TimeSource::MANUAL;
    status_.last_sync = t;
    return Result<void>::Ok();
}

bool TimeService::get_local_time(struct tm& out){
    time_t now; time(&now);
    if(now < 1000000) return false;
    localtime_r(&now, &out);
    return true;
}

void TimeService::get_formatted(char* time_buf, size_t time_len, char* date_buf, size_t date_len, bool use_24h){
    struct tm ti{};
    if(!get_local_time(ti)){
        snprintf(time_buf, time_len, "--:--");
        snprintf(date_buf, date_len, "-- --- ----");
        return;
    }
    if(use_24h){
        strftime(time_buf, time_len, "%H:%M:%S", &ti);
    } else {
        strftime(time_buf, time_len, "%I:%M %p", &ti);
    }
    strftime(date_buf, date_len, "%d %b %Y", &ti);
}

} // namespace time_service
} // namespace smart_device

#include "settings_store.h"
#include "common/string_utils.h"
#include "esp_log.h"

static const char* TAG = "SETTINGS";

namespace smart_device {
namespace storage {

SettingsStore& SettingsStore::instance(){ static SettingsStore s; return s; }

bool SettingsStore::load(){
    auto& nvs = NvsStore::instance();
    if(!nvs.is_initialized()) nvs.initialize();

    if(auto r = nvs.get_string("device_name"); r.is_ok()){
        copy_cstr(settings_.device_name, r.value().c_str());
    }
    if(auto r = nvs.get_string("wifi_ssid"); r.is_ok()){
        copy_cstr(settings_.wifi_ssid, r.value().c_str());
    }
    if(auto r = nvs.get_string("wifi_pass"); r.is_ok()){
        copy_cstr(settings_.wifi_password, r.value().c_str());
    }
    if(auto r = nvs.get_string("timezone"); r.is_ok()){
        copy_cstr(settings_.timezone, r.value().c_str());
    }
    if(auto r = nvs.get_i32("brightness"); r.is_ok()){
        settings_.display_brightness = (uint8_t)r.value();
    }
    if(auto r = nvs.get_bool("ble_en"); r.is_ok()) settings_.ble_enabled = r.value();
    if(auto r = nvs.get_bool("wifi_en"); r.is_ok()) settings_.wifi_enabled = r.value();
    if(auto r = nvs.get_bool("time_24h"); r.is_ok()) settings_.time_24h = r.value();

    ESP_LOGI(TAG, "Settings loaded: device=%s ssid=%s tz=%s brightness=%d",
             settings_.device_name, settings_.wifi_ssid, settings_.timezone, settings_.display_brightness);
    return true;
}

bool SettingsStore::save(){
    auto& nvs = NvsStore::instance();
    bool ok = true;
    auto chk = [&](Result<void> r, const char* key){
        if (r.is_err()) { ESP_LOGE(TAG, "save %s failed: %s", key, r.error_message().c_str()); ok = false; }
    };
    chk(nvs.set_string("device_name", settings_.device_name), "device_name");
    chk(nvs.set_string("wifi_ssid", settings_.wifi_ssid), "wifi_ssid");
    chk(nvs.set_string("wifi_pass", settings_.wifi_password), "wifi_pass");
    chk(nvs.set_string("timezone", settings_.timezone), "timezone");
    chk(nvs.set_i32("brightness", settings_.display_brightness), "brightness");
    chk(nvs.set_bool("ble_en", settings_.ble_enabled), "ble_en");
    chk(nvs.set_bool("wifi_en", settings_.wifi_enabled), "wifi_en");
    chk(nvs.set_bool("time_24h", settings_.time_24h), "time_24h");
    if (ok) ESP_LOGI(TAG, "Settings saved");
    else ESP_LOGW(TAG, "Settings save partial failure — NVS may be worn");
    return ok;
}

bool SettingsStore::factory_reset(){
    copy_cstr(settings_.device_name, "DEV-S3-001");
    settings_.wifi_ssid[0]='\0';
    settings_.wifi_password[0]='\0';
    copy_cstr(settings_.timezone, "IST-5:30");
    settings_.display_brightness=80;
    settings_.ble_enabled=true;
    settings_.wifi_enabled=true;
    settings_.time_24h=false;
    save();
    ESP_LOGI(TAG, "Factory reset done");
    return true;
}

} // namespace storage
} // namespace smart_device

#include "wifi_service.h"
#include "common/string_utils.h"
#include "storage/settings_store.h"
#include "event_bus/event_bus.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "mk.h"

static const char* TAG = "WIFI_SERVICE";
static mk_event_group_t* wifi_event_group;
const int WIFI_CONNECTED_BIT = BIT0;
const int WIFI_FAIL_BIT = BIT1;

namespace smart_device {
namespace connectivity {

WifiService& WifiService::instance(){ static WifiService s; return s; }

void WifiService::event_handler(void* arg, esp_event_base_t base, int32_t id, void* data){
    auto& self = WifiService::instance();
    if(base==WIFI_EVENT){
        if(id==WIFI_EVENT_STA_START){
            esp_wifi_connect();
            self.status_.state = WifiState::CONNECTING;
            AppEvent ev{AppEventType::WIFI_CONNECTING, 0, 0, "WiFi connecting"}; EventBus::instance().publish(ev);
        } else if(id==WIFI_EVENT_STA_DISCONNECTED){
            self.status_.state = WifiState::DISCONNECTED;
            self.status_.has_ip = false;
            if(wifi_event_group) mk_event_set(wifi_event_group, WIFI_FAIL_BIT);
            AppEvent ev{AppEventType::WIFI_DISCONNECTED, 0, 0, "WiFi disconnected"}; EventBus::instance().publish(ev);
            ESP_LOGI(TAG, "WiFi disconnected, retry...");
            esp_wifi_connect();
        } else if(id==WIFI_EVENT_SCAN_DONE){
            AppEvent ev{AppEventType::WIFI_SCAN_DONE}; EventBus::instance().publish(ev);
        }
    } else if(base==IP_EVENT && id==IP_EVENT_STA_GOT_IP){
        ip_event_got_ip_t* event = (ip_event_got_ip_t*)data;
        snprintf(self.status_.ip, sizeof(self.status_.ip), IPSTR, IP2STR(&event->ip_info.ip));
        self.status_.state = WifiState::CONNECTED;
        self.status_.has_ip = true;
        wifi_ap_record_t ap; esp_wifi_sta_get_ap_info(&ap); self.status_.rssi = ap.rssi;
        if(wifi_event_group) mk_event_set(wifi_event_group, WIFI_CONNECTED_BIT);
        AppEvent ev{AppEventType::WIFI_CONNECTED, 0, self.status_.rssi, ""}; 
        snprintf(ev.message, sizeof(ev.message), "Connected %.53s", self.status_.ssid);
        EventBus::instance().publish(ev);
        ESP_LOGI(TAG, "WiFi connected IP=%s RSSI=%d", self.status_.ip, self.status_.rssi);
    }
}

Result<void> WifiService::initialize(){
    if(initialized_) return Result<void>::Ok();
    ESP_LOGI(TAG, "Initializing WiFi");

    wifi_event_group = mk_event_group_create("wifi");
    if(!wifi_event_group){
        return Result<void>::Err(AppError::WIFI_NOT_INITIALIZED, "event group allocation failed");
    }
    esp_netif_t* sta_netif = esp_netif_create_default_wifi_sta();
    if(!sta_netif){
        return Result<void>::Err(AppError::WIFI_NOT_INITIALIZED, "STA netif creation failed");
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_err_t ret = esp_wifi_init(&cfg);
    if(ret!=ESP_OK) return Result<void>::Err(AppError::WIFI_NOT_INITIALIZED, esp_err_to_name(ret));

    esp_event_handler_instance_t instance_any_id, instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, nullptr, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, nullptr, &instance_got_ip));

    wifi_config_t wifi_config{};
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;

    ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if(ret!=ESP_OK) return Result<void>::Err(AppError::WIFI_NOT_INITIALIZED, esp_err_to_name(ret));
    ret = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if(ret!=ESP_OK) return Result<void>::Err(AppError::WIFI_NOT_INITIALIZED, esp_err_to_name(ret));

    initialized_ = true;
    status_.state = WifiState::IDLE;
    ESP_LOGI(TAG, "WiFi initialized");
    return Result<void>::Ok();
}

Result<void> WifiService::start(){
    if(!initialized_) { auto r=initialize(); if(r.is_err()) return r; }
    esp_err_t ret = esp_wifi_start();
    if(ret!=ESP_OK) return Result<void>::Err(AppError::WIFI_NOT_INITIALIZED, esp_err_to_name(ret));
    status_.state = WifiState::CONNECTING;
    ESP_LOGI(TAG, "WiFi started");
    return Result<void>::Ok();
}

Result<void> WifiService::stop(){
    esp_wifi_stop();
    status_.state = WifiState::IDLE;
    return Result<void>::Ok();
}

Result<void> WifiService::connect(const std::string& ssid, const std::string& pass, uint32_t timeout_ms){
    if(!initialized_) initialize();
    ESP_LOGI(TAG, "Connecting to %s", ssid.c_str());

    wifi_config_t cfg{};
    copy_cstr(reinterpret_cast<char*>(cfg.sta.ssid), sizeof(cfg.sta.ssid), ssid.c_str());
    copy_cstr(reinterpret_cast<char*>(cfg.sta.password), sizeof(cfg.sta.password), pass.c_str());
    cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    esp_err_t ret = esp_wifi_set_config(WIFI_IF_STA, &cfg);
    if(ret!=ESP_OK) return Result<void>::Err(AppError::WIFI_NOT_INITIALIZED, esp_err_to_name(ret));

    copy_cstr(status_.ssid, ssid.c_str());
    status_.state = WifiState::CONNECTING;

    mk_event_clear(wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);
    ret = esp_wifi_connect();
    if(ret!=ESP_OK){
        status_.state = WifiState::FAILED;
        return Result<void>::Err(AppError::WIFI_TIMEOUT, esp_err_to_name(ret));
    }

    mk_event_bits_t bits = mk_event_wait(wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, false, false, timeout_ms);
    if(bits & WIFI_CONNECTED_BIT){
        status_.state = WifiState::CONNECTED;
        // save to NVS
        auto& settings = storage::SettingsStore::instance().settings();
        copy_cstr(settings.wifi_ssid, ssid.c_str());
        copy_cstr(settings.wifi_password, pass.c_str());
        storage::SettingsStore::instance().save();
        return Result<void>::Ok();
    } else {
        status_.state = WifiState::FAILED;
        return Result<void>::Err(AppError::WIFI_TIMEOUT, "Connection timeout or auth failed");
    }
}

Result<void> WifiService::disconnect(){
    esp_wifi_disconnect();
    status_.state = WifiState::DISCONNECTED;
    status_.has_ip = false;
    return Result<void>::Ok();
}

Result<std::vector<WifiApRecord>> WifiService::scan(uint32_t timeout_ms){
    ESP_LOGI(TAG, "WiFi scan started");
    status_.state = WifiState::SCANNING;
    wifi_scan_config_t scan_cfg{};
    scan_cfg.show_hidden = true;
    scan_cfg.scan_type = WIFI_SCAN_TYPE_ACTIVE;
    scan_cfg.scan_time.active.min = 100;
    scan_cfg.scan_time.active.max = 300;

    esp_err_t ret = esp_wifi_scan_start(&scan_cfg, true);
    if(ret!=ESP_OK){
        status_.state = WifiState::FAILED;
        return Result<std::vector<WifiApRecord>>::Err(AppError::WIFI_SCAN_FAILED, esp_err_to_name(ret));
    }
    uint16_t num = 0;
    esp_wifi_scan_get_ap_num(&num);
    std::vector<wifi_ap_record_t> records(num);
    if(num>0) esp_wifi_scan_get_ap_records(&num, records.data());

    std::vector<WifiApRecord> result;
    for(auto &r: records){
        WifiApRecord ap{};
        copy_cstr(ap.ssid, reinterpret_cast<const char*>(r.ssid));
        ap.rssi=r.rssi; ap.auth=r.authmode; ap.channel=r.primary;
        result.push_back(ap);
    }
    status_.state = WifiState::IDLE;
    ESP_LOGI(TAG, "Scan done %d APs", (int)result.size());
    return Result<std::vector<WifiApRecord>>::Ok(result);
}

} // namespace connectivity
} // namespace smart_device

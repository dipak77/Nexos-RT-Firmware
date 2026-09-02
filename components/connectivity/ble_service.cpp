#include "ble_service.h"
#include "common/string_utils.h"
#include "storage/settings_store.h"
#include "event_bus/event_bus.h"
#include "esp_log.h"
#include <cstring>
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

static const char* TAG = "BLE_SERVICE";

namespace smart_device {
namespace connectivity {

static void ble_on_sync(void);
static void ble_on_reset(int reason);
static int ble_gap_event(struct ble_gap_event* event, void* arg);

BleService& BleService::instance(){ static BleService s; return s; }

Result<void> BleService::initialize(){
    if(initialized_) return Result<void>::Ok();
    ESP_LOGI(TAG, "Initializing NimBLE");

    esp_err_t ret = nimble_port_init();
    if(ret!=ESP_OK){
        ESP_LOGE(TAG, "NimBLE port init failed: %s", esp_err_to_name(ret));
        return Result<void>::Err(AppError::BLE_INIT_FAILED, esp_err_to_name(ret));
    }

    ble_hs_cfg.sync_cb = ble_on_sync;
    ble_hs_cfg.reset_cb = ble_on_reset;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    ble_svc_gap_init();
    ble_svc_gatt_init();

    auto& settings = storage::SettingsStore::instance().settings();
    const char* name = settings.ble_device_name[0] ? settings.ble_device_name : "SmartDisplay-BLE";
    ble_svc_gap_device_name_set(name);

    status_.state = BleState::INITIALIZED;
    copy_cstr(status_.device_name, name);
    // Publish initialized state before starting the host task. NimBLE may call
    // ble_on_sync() immediately; that callback starts advertising and must not
    // recursively enter initialize() while the port is already active.
    initialized_ = true;

    nimble_port_freertos_init([](void* arg){ nimble_port_run(); return; });

    ESP_LOGI(TAG, "BLE initialized name=%s", name);
    return Result<void>::Ok();
}

Result<void> BleService::start_advertising(){
    if(!initialized_){ auto r=initialize(); if(r.is_err()) return r; }
    struct ble_gap_adv_params adv_params{};
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    // Fast advertising so Android/iOS scanners see the device quickly.
    adv_params.itvl_min = 0x20; // 20 ms
    adv_params.itvl_max = 0x40; // 40 ms

    // Keep the complete local name in the primary ADV PDU. iOS Settings uses
    // passive scan and will not pick up a name that only lives in scan response.
    const char* name = status_.device_name[0] ? status_.device_name : "SmartDisplay";
    const char* adv_name = name;
    uint8_t adv_name_len = static_cast<uint8_t>(strlen(adv_name));
    bool complete = true;
    // Flags(3) + name(2+len) must stay <= 31. Truncate rather than drop the name.
    if(adv_name_len > 20){
        adv_name_len = 20;
        complete = false;
    }

    struct ble_hs_adv_fields fields{};
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.name = (uint8_t*)adv_name;
    fields.name_len = adv_name_len;
    fields.name_is_complete = complete ? 1 : 0;

    int rc = ble_gap_adv_set_fields(&fields);
    if(rc != 0){
        ESP_LOGW(TAG, "ADV fields rc=%d, retrying with short name", rc);
        static const char* short_name = "SmartDisplay";
        fields.name = (uint8_t*)short_name;
        fields.name_len = 12;
        fields.name_is_complete = 1;
        rc = ble_gap_adv_set_fields(&fields);
        if(rc != 0){
            ESP_LOGE(TAG, "ADV fields failed rc=%d", rc);
            status_.state = BleState::FAILED;
            return Result<void>::Err(AppError::BLE_ADVERTISING_FAILED, "adv fields failed");
        }
    }

    if(ble_gap_adv_active()){
        ble_gap_adv_stop();
    }

    uint8_t own_addr_type = BLE_OWN_ADDR_PUBLIC;
    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if(rc != 0){
        ESP_LOGW(TAG, "addr infer rc=%d, using random", rc);
        own_addr_type = BLE_OWN_ADDR_RANDOM;
    }

    rc = ble_gap_adv_start(own_addr_type, nullptr, BLE_HS_FOREVER, &adv_params, ble_gap_event, nullptr);
    if(rc!=0){
        ESP_LOGE(TAG, "Adv start failed rc=%d", rc);
        status_.state = BleState::FAILED;
        return Result<void>::Err(AppError::BLE_ADVERTISING_FAILED, "adv start failed");
    }
    status_.advertising = true;
    status_.state = BleState::ADVERTISING;
    ESP_LOGI(TAG, "BLE advertising started name=%s", name);

    AppEvent ev{AppEventType::BLE_STARTED}; EventBus::instance().publish(ev);
    return Result<void>::Ok();
}

Result<void> BleService::stop_advertising(){
    ble_gap_adv_stop();
    status_.advertising = false;
    status_.state = BleState::INITIALIZED;
    return Result<void>::Ok();
}

Result<void> BleService::stop(){
    ble_gap_adv_stop();
    nimble_port_stop();
    status_.state = BleState::STOPPED;
    status_.advertising = false;
    AppEvent ev{AppEventType::BLE_STOPPED}; EventBus::instance().publish(ev);
    return Result<void>::Ok();
}

void BleService::set_connected(bool connected){
    status_.connected = connected;
    status_.advertising = !connected;
    status_.state = connected ? BleState::CONNECTED : BleState::ADVERTISING;
}

static void ble_on_sync(void){
    ESP_LOGI(TAG, "BLE sync on core %d", mk_current_core());
    int rc = ble_hs_util_ensure_addr(0);
    if(rc != 0){
        ESP_LOGE(TAG, "BLE address ensure failed rc=%d", rc);
        return;
    }
    // NimBLE host task (Core0) context — publish only, don't block. start_advertising is lightweight.
    BleService::instance().start_advertising();
}
static void ble_on_reset(int reason){
    ESP_LOGW(TAG, "BLE reset reason=%d", reason);
}

static int ble_gap_event(struct ble_gap_event* event, void* arg){
    auto& svc = BleService::instance();
    switch(event->type){
        case BLE_GAP_EVENT_CONNECT: {
            if(event->connect.status==0){
                ESP_LOGI(TAG, "BLE connected");
                svc.set_connected(true);
                AppEvent ev{AppEventType::BLE_CONNECTED}; EventBus::instance().publish(ev);
            } else {
                ESP_LOGI(TAG, "BLE connect failed, restart adv");
                BleService::instance().start_advertising();
            }
            break;
        }
        case BLE_GAP_EVENT_DISCONNECT: {
            ESP_LOGI(TAG, "BLE disconnected reason=%d", event->disconnect.reason);
            svc.set_connected(false);
            AppEvent ev2{AppEventType::BLE_DISCONNECTED}; EventBus::instance().publish(ev2);
            BleService::instance().start_advertising();
            break;
        }
        case BLE_GAP_EVENT_ADV_COMPLETE:
            ESP_LOGI(TAG, "Adv complete, restart");
            BleService::instance().start_advertising();
            break;
        default: break;
    }
    return 0;
}

} // namespace connectivity
} // namespace smart_device

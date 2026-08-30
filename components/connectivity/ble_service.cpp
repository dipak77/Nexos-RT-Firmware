#include "ble_service.h"
#include "common/string_utils.h"
#include "storage/settings_store.h"
#include "event_bus/event_bus.h"
#include "esp_log.h"
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

    struct ble_hs_adv_fields fields{};
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.tx_pwr_lvl_is_present = 1;
    fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;
    const char* name = status_.device_name;
    fields.name = (uint8_t*)name;
    fields.name_len = strlen(name);
    fields.name_is_complete = 1;

    ble_gap_adv_set_fields(&fields);

    int rc = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, nullptr, BLE_HS_FOREVER, &adv_params, ble_gap_event, nullptr);
    if(rc!=0){
        ESP_LOGE(TAG, "Adv start failed rc=%d", rc);
        status_.state = BleState::FAILED;
        return Result<void>::Err(AppError::BLE_ADVERTISING_FAILED, "adv start failed");
    }
    status_.advertising = true;
    status_.state = BleState::ADVERTISING;
    ESP_LOGI(TAG, "BLE advertising started");

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

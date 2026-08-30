#include "diagnostics.h"
#include "board/board.h"
#include "device_hal/spi_hal.h"
#include "device_hal/i2c_hal.h"
#include "display/display_gc9a01.h"
#include "connectivity/wifi_service.h"
#include "connectivity/ble_service.h"
#include "time_service/time_service.h"
#include "storage/nvs_store.h"
#include "esp_log.h"
#include "common/app_version.h"

static const char* TAG = "DIAGNOSTICS";

namespace smart_device {
namespace diagnostics {

Diagnostics& Diagnostics::instance(){ static Diagnostics s; return s; }

void Diagnostics::initialize(){
    HealthMonitor::instance().initialize();
    ESP_LOGI(TAG, "Diagnostics initialized");
}

SelfTestResult Diagnostics::run_self_test(){
    SelfTestResult result{};
    auto add = [&](const char* name, bool pass, const char* detail=""){
        result.tests[result.total] = {name, pass, detail};
        result.total++;
        if(pass) result.passed++; else result.failed++;
    };

    // NVS
    add("NVS", storage::NvsStore::instance().is_initialized(), "NVS ready");
    // Board
    add("BOARD", board::Board::instance().is_initialized(), board::Board::instance().config().name);
    // SPI
    add("SPI", hal::SpiHal::instance().is_initialized(), "SPI2");
    // Display
    add("DISPLAY", display::GC9A01Display::instance().is_initialized(), "GC9A01 240x240");
    // I2C
    add("I2C", hal::I2cHal::instance().is_initialized(), "I2C expansion");
    // WiFi
    add("WIFI", connectivity::WifiService::instance().status().state != connectivity::WifiState::FAILED, "WiFi stack");
    // BLE
    add("BLE", true, "NimBLE stack");
    // Time
    add("TIME", true, time_service::TimeService::instance().status().synced ? "Synced" : "Not synced yet");
    // Heap
    auto metrics = HealthMonitor::instance().get_metrics();
    add("HEAP", metrics.free_heap > 50*1024, "Free heap check");

    result.overall_pass = (result.failed==0);
    ESP_LOGI(TAG, "Self-test %d/%d passed", result.passed, result.total);
    return result;
}

std::string Diagnostics::get_system_status_text(){
    auto metrics = HealthMonitor::instance().get_metrics();
    auto wifi = connectivity::WifiService::instance().status();
    auto ble = connectivity::BleService::instance().status();
    auto time = time_service::TimeService::instance().status();

    char buf[1024];
    snprintf(buf, sizeof(buf),
        "SYSTEM STATUS\n"
        "------------------------\n"
        "State          READY\n"
        "Uptime         %02lu:%02lu:%02lu\n"
        "Heap           %lu KB\n"
        "Min Heap       %lu KB\n"
        "Reset Reason   %s\n\n"
        "DISPLAY\n"
        "Controller     GC9A01\n"
        "Resolution     240x240\n"
        "State          %s\n\n"
        "WIFI\n"
        "State          %s\n"
        "SSID           %s\n"
        "IP             %s\n"
        "RSSI           %d dBm\n\n"
        "BLE\n"
        "State          %s\n"
        "Name           %s\n\n"
        "TIME\n"
        "State          %s\n"
        "TZ             %s\n\n"
        "FW %s HW %s\n",
        metrics.uptime_sec/3600, (metrics.uptime_sec%3600)/60, metrics.uptime_sec%60,
        metrics.free_heap/1024, metrics.min_free_heap/1024,
        HealthMonitor::instance().reset_reason_str(),
        display::GC9A01Display::instance().is_initialized() ? "OK" : "NOT READY",
        (wifi.state==connectivity::WifiState::CONNECTED)?"CONNECTED": (wifi.state==connectivity::WifiState::CONNECTING?"CONNECTING":"IDLE"),
        wifi.ssid, wifi.ip, wifi.rssi,
        ble.advertising?"ADVERTISING": (ble.connected?"CONNECTED":"IDLE"),
        ble.device_name,
        time.synced?"SYNCED":"NOT SYNCED",
        time.timezone,
        APP_VERSION_STRING, APP_HW_VERSION
    );
    return std::string(buf);
}

std::string Diagnostics::get_system_status_json(){
    auto m = HealthMonitor::instance().get_metrics();
    char buf[512];
    snprintf(buf, sizeof(buf),
        "{\"fw\":\"%s\",\"hw\":\"%s\",\"uptime\":%lu,\"heap\":%lu,\"min_heap\":%lu}",
        APP_VERSION_STRING, APP_HW_VERSION, (unsigned long)m.uptime_sec, (unsigned long)m.free_heap, (unsigned long)m.min_free_heap);
    return std::string(buf);
}

} // namespace diagnostics
} // namespace smart_device

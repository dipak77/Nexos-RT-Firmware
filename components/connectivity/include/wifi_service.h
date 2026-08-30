#pragma once
#include "connection_state.h"
#include "common/result.h"
#include "esp_wifi.h"
#include <string>
#include <vector>

namespace smart_device {
namespace connectivity {

struct WifiApRecord {
    char ssid[33];
    int rssi;
    wifi_auth_mode_t auth;
    uint8_t channel;
};

class WifiService {
public:
    static WifiService& instance();
    Result<void> initialize();
    Result<void> start();
    Result<void> stop();
    Result<void> connect(const std::string& ssid, const std::string& pass, uint32_t timeout_ms = 15000);
    Result<void> disconnect();
    Result<std::vector<WifiApRecord>> scan(uint32_t timeout_ms = 10000);

    WifiStatus status() const { return status_; }
    bool is_connected() const { return status_.state == WifiState::CONNECTED; }

private:
    WifiService() = default;
    WifiStatus status_{};
    bool initialized_{false};
    static void event_handler(void* arg, esp_event_base_t base, int32_t id, void* data);
};

} // namespace connectivity
} // namespace smart_device

#pragma once
#include <cstdint>

namespace smart_device {
namespace connectivity {

enum class WifiState { IDLE, SCANNING, CONNECTING, CONNECTED, DISCONNECTED, FAILED };
enum class BleState { IDLE, INITIALIZED, ADVERTISING, CONNECTED, STOPPED, FAILED };

struct WifiStatus {
    WifiState state{WifiState::IDLE};
    char ssid[64]{""};
    char ip[16]{""};
    int rssi{0};
    bool has_ip{false};
    char ap_ssid[33]{""};
    char ap_ip[16]{"192.168.4.1"};
    bool ap_running{false};
    uint8_t ap_clients{0};
};

struct BleStatus {
    BleState state{BleState::IDLE};
    char device_name[32]{"SmartDisplay-BLE"};
    bool advertising{false};
    bool connected{false};
};

} // namespace connectivity
} // namespace smart_device

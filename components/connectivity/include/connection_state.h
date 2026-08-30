#pragma once

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
};

struct BleStatus {
    BleState state{BleState::IDLE};
    char device_name[32]{"SmartDisplay-BLE"};
    bool advertising{false};
    bool connected{false};
};

} // namespace connectivity
} // namespace smart_device

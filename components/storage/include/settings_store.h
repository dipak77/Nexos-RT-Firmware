#pragma once
#include "nvs_store.h"
#include <string>

namespace smart_device {
namespace storage {

struct DeviceSettings {
    char device_name[32]{"DEV-S3-001"};
    char wifi_ssid[64]{""};
    char wifi_password[64]{""};
    char timezone[32]{"IST-5:30"};
    uint8_t display_brightness{80};
    bool ble_enabled{true};
    bool wifi_enabled{true};
    bool time_24h{false}; // false = 12h with AM/PM for India
    char ble_device_name[32]{"SmartDisplay-BLE"};
};

class SettingsStore {
public:
    static SettingsStore& instance();
    bool load();
    bool save();
    bool factory_reset();
    DeviceSettings& settings() { return settings_; }
    const DeviceSettings& settings() const { return settings_; }

private:
    DeviceSettings settings_{};
};

} // namespace storage
} // namespace smart_device

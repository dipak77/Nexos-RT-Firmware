#pragma once
#include <cstdint>
#include <string>

namespace smart_device {

enum class AppEventType : uint32_t {
    SYSTEM_BOOTED = 0x0001,
    DISPLAY_READY = 0x0002,

    WIFI_CONNECTING = 0x0101,
    WIFI_CONNECTED = 0x0102,
    WIFI_DISCONNECTED = 0x0103,
    WIFI_SCAN_DONE = 0x0104,

    BLE_STARTED = 0x0201,
    BLE_STOPPED = 0x0202,
    BLE_CONNECTED = 0x0203,
    BLE_DISCONNECTED = 0x0204,

    TIME_SYNC_STARTED = 0x0301,
    TIME_SYNCED = 0x0302,
    TIME_SYNC_FAILED = 0x0303,

    COMMAND_STARTED = 0x0401,
    COMMAND_SUCCESS = 0x0402,
    COMMAND_FAILED = 0x0403,

    OTA_STARTED = 0x0501,
    OTA_PROGRESS = 0x0502,
    OTA_COMPLETE = 0x0503,
    OTA_FAILED = 0x0504,

    DIAGNOSTICS_UPDATED = 0x0601,
    SYSTEM_ERROR = 0x0701,
};

struct AppEvent {
    AppEventType type;
    uint32_t code{0};
    int32_t value{0};
    char message[64]{};
    uint64_t timestamp_ms{0};
};

} // namespace smart_device

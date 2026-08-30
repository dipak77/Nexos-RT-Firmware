#pragma once
#include <string>
#include <cstdint>

namespace smart_device {

enum class CommandStatus { SUCCESS, FAILED, INVALID, TIMEOUT, BUSY, WORKING };

enum class CommandId : uint32_t {
    GET_VERSION = 0x0001,
    GET_STATUS = 0x0002,
    REBOOT = 0x0003,
    SELF_TEST = 0x0501,
    FACTORY_RESET = 0x0004,

    WIFI_STATUS = 0x0101,
    WIFI_SCAN = 0x0102,
    WIFI_CONNECT = 0x0103,
    WIFI_DISCONNECT = 0x0104,

    BLE_STATUS = 0x0201,
    BLE_START = 0x0202,
    BLE_STOP = 0x0203,

    TIME_GET = 0x0301,
    TIME_SYNC = 0x0302,
    TIME_SET = 0x0303,

    DISPLAY_TEST = 0x0401,
    DISPLAY_BRIGHTNESS = 0x0402,
    DISPLAY_CLEAR = 0x0403,

    OTA_STATUS = 0x0601,
    OTA_UPDATE = 0x0602,

    SYSTEM_INFO = 0x0701,
    RESET_INFO = 0x0702,
    HELP = 0xFFFF
};

struct CommandResult {
    uint32_t id{0};
    CommandId command_id{CommandId::GET_STATUS};
    CommandStatus status{CommandStatus::SUCCESS};
    std::string command;
    std::string message;
    int error_code{0};
    uint32_t execution_time_ms{0};
};

namespace command {
const char* command_status_to_string(CommandStatus s);
const char* command_id_to_string(CommandId id);
} // namespace command

} // namespace smart_device

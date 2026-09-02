#pragma once
#include "common/result.h"
#include "command/command_result.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include <ctime>
#include <cstdint>

namespace smart_device {
namespace gui {

struct UiState {
    char time_str[16]{"--:--"};
    char date_str[16]{"-- --- ----"};
    char time_full[16]{"--:--:--"}; // 24h internal
    bool wifi_connected{false};
    bool wifi_ap_running{false};
    int wifi_rssi{0};
    char wifi_ssid[32]{""};
    char wifi_ap_ssid[32]{""};
    char ip[16]{""};

    bool ble_enabled{false};
    bool ble_connected{false};
    bool ble_advertising{false};

    bool time_synced{false};
    char latest_command[32]{"READY"};
    CommandStatus command_status{CommandStatus::SUCCESS};
    char command_message[64]{"SYSTEM READY"};
    uint32_t uptime_sec{0};
    uint32_t free_heap{0};
    char firmware_version[16]{"1.2.0"};
    char system_status[16]{"SYSTEM OK"};
    uint8_t brightness{80};
    bool show_splash{false};
    uint8_t cpu_load{0};
    char kernel_version[16]{"1.2.0"};
};

class Gui {
public:
    static Gui& instance();
    Result<void> initialize(esp_lcd_panel_handle_t panel, esp_lcd_panel_io_handle_t io);
    Result<void> start();
    void update_state(const UiState& state);
    UiState& state() { return ui_state_; }
    void show_command_result(const char* cmd, CommandStatus status, const char* msg, uint32_t time_ms);
    void show_splash(bool show);
    bool is_initialized() const { return initialized_; }

private:
    Gui() = default;
    bool initialized_{false};
    UiState ui_state_{};
};

} // namespace gui
} // namespace smart_device

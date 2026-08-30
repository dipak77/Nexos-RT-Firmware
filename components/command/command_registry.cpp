#include "command_registry.h"
#include "mk.h"
#include "esp_timer.h"
#include "common/app_version.h"
#include "storage/settings_store.h"
#include "connectivity/wifi_service.h"
#include "connectivity/ble_service.h"
#include "time_service/time_service.h"
#include "display/display_gc9a01.h"
#include "diagnostics/diagnostics.h"
#include "ota/ota_service.h"
#include "board/board.h"
#include "device_hal/i2c_hal.h"
#include "gui/gui.h"
#include "gui/gui_command.h"
#include "lvgl_adapter/lvgl_adapter.h"
#include "event_bus/event_bus.h"
#include <sstream>
#include <algorithm>

namespace smart_device {
namespace command {

const char* command_status_to_string(CommandStatus s){
    switch(s){
        case CommandStatus::SUCCESS: return "SUCCESS";
        case CommandStatus::FAILED: return "FAILED";
        case CommandStatus::INVALID: return "INVALID";
        case CommandStatus::TIMEOUT: return "TIMEOUT";
        case CommandStatus::BUSY: return "BUSY";
        case CommandStatus::WORKING: return "WORKING";
        default: return "UNKNOWN";
    }
}
const char* command_id_to_string(CommandId id){
    switch(id){
        case CommandId::GET_VERSION: return "GET_VERSION";
        case CommandId::GET_STATUS: return "GET_STATUS";
        case CommandId::WIFI_STATUS: return "WIFI_STATUS";
        case CommandId::BLE_STATUS: return "BLE_STATUS";
        default: return "CMD";
    }
}

CommandRegistry& CommandRegistry::instance(){ static CommandRegistry s; return s; }

bool CommandRegistry::register_command(const CommandInfo& info){
    commands_[info.name] = info;
    // also register short aliases? Keep simple
    return true;
}

bool CommandRegistry::has_command(const std::string& name) const{
    return commands_.find(name)!=commands_.end();
}

std::vector<CommandInfo> CommandRegistry::list_commands() const{
    std::vector<CommandInfo> list;
    for(auto &kv: commands_) list.push_back(kv.second);
    return list;
}

CommandResult CommandRegistry::execute(const std::string& line){
    auto start = esp_timer_get_time();
    std::istringstream iss(line);
    std::vector<std::string> tokens;
    std::string tok;
    while(iss>>tok) tokens.push_back(tok);
    if(tokens.empty()){
        return CommandResult{0, CommandId::HELP, CommandStatus::INVALID, line, "Empty command", 4002, 0};
    }
    std::string cmd_name = tokens[0];
    std::transform(cmd_name.begin(), cmd_name.end(), cmd_name.begin(), ::tolower);

    // Handle compound commands like "wifi status" -> "wifi_status" lookup
    std::string full = cmd_name;
    if(tokens.size()>=2){
        std::string combined = tokens[0] + " " + tokens[1];
        std::string combined_key = tokens[0] + "_" + tokens[1];
        std::transform(combined_key.begin(), combined_key.end(), combined_key.begin(), ::tolower);
        if(has_command(combined_key)){
            cmd_name = combined_key;
            // shift args: remove first two, keep rest
            std::vector<std::string> new_args(tokens.begin()+2, tokens.end());
            tokens = new_args;
            // Need to search again
            auto it = commands_.find(cmd_name);
            if(it!=commands_.end()){
                auto result = it->second.handler(tokens);
                result.command = line;
                result.id = (uint32_t)it->second.id;
                result.command_id = it->second.id;
                result.execution_time_ms = (esp_timer_get_time()-start)/1000;
                return result;
            }
        }
    }

    auto it = commands_.find(cmd_name);
    if(it==commands_.end()){
        // try with underscore vs space? Already tried
        // Check help
        if(cmd_name=="help" || cmd_name=="?"){
            std::string msg = "Available commands:\n";
            for(auto &kv: commands_) msg += "  " + kv.second.name + " - " + kv.second.description + "\n";
            CommandResult r{0, CommandId::HELP, CommandStatus::SUCCESS, line, msg, 0, (uint32_t)((esp_timer_get_time()-start)/1000)};
            return r;
        }
        CommandResult r{0, CommandId::GET_STATUS, CommandStatus::INVALID, line, "Unknown command: " + cmd_name + ". Type 'help'", 4001, (uint32_t)((esp_timer_get_time()-start)/1000)};
        return r;
    }

    std::vector<std::string> args(tokens.begin()+1, tokens.end());
    CommandResult res = it->second.handler(args);
    res.command = line;
    res.id = (uint32_t)it->second.id;
    res.command_id = it->second.id;
    res.execution_time_ms = (esp_timer_get_time()-start)/1000;
    return res;
}

void CommandRegistry::register_builtin_commands(){
    // system info
    register_command({CommandId::GET_VERSION, "version", "Show firmware version", "version", [](auto args){
        char buf[256];
        snprintf(buf, sizeof(buf),
            "Product       %s\nModel         %s\nFirmware      %s\nHardware      %s\nNexos-RT      %s\nESP-IDF       %s\nBuild         %s\nCommit        %s\n",
            APP_NAME, APP_MODEL, APP_VERSION_STRING, APP_HW_VERSION,
            MK_CONFIG_VERSION_STRING, IDF_TARGET_VERSION, BUILD_TIMESTAMP, GIT_COMMIT);
        return CommandResult{0, CommandId::GET_VERSION, CommandStatus::SUCCESS, "version", buf, 0, 0};
    }});

    register_command({CommandId::GET_STATUS, "status", "Show system status", "status", [](auto args){
        auto txt = diagnostics::Diagnostics::instance().get_system_status_text();
        return CommandResult{0, CommandId::GET_STATUS, CommandStatus::SUCCESS, "status", txt, 0, 0};
    }});
    register_command({CommandId::GET_STATUS, "system_info", "System info", "system info", [](auto args){
        auto txt = diagnostics::Diagnostics::instance().get_system_status_text();
        return CommandResult{0, CommandId::SYSTEM_INFO, CommandStatus::SUCCESS, "system info", txt, 0, 0};
    }});
    register_command({CommandId::GET_STATUS, "system_status", "System status", "system status", [](auto args){
        auto txt = diagnostics::Diagnostics::instance().get_system_status_text();
        return CommandResult{0, CommandId::GET_STATUS, CommandStatus::SUCCESS, "system status", txt, 0, 0};
    }});

    // wifi
    register_command({CommandId::WIFI_STATUS, "wifi_status", "WiFi status", "wifi status", [](auto args){
        auto st = connectivity::WifiService::instance().status();
        char buf[256];
        snprintf(buf, sizeof(buf), "[WIFI]\nstate : %s\nssid : %s\nrssi : %d dBm\nip : %s\n",
            (st.state==connectivity::WifiState::CONNECTED)?"CONNECTED": (st.state==connectivity::WifiState::CONNECTING?"CONNECTING":"DISCONNECTED"),
            st.ssid, st.rssi, st.ip);
        return CommandResult{0, CommandId::WIFI_STATUS, CommandStatus::SUCCESS, "wifi status", buf, 0, 0};
    }});
    register_command({CommandId::WIFI_SCAN, "wifi_scan", "Scan WiFi APs", "wifi scan", [](auto args){
        auto res = connectivity::WifiService::instance().scan();
        if(res.is_err()) return CommandResult{0, CommandId::WIFI_SCAN, CommandStatus::FAILED, "wifi scan", res.error_message(), (int)res.error(), 0};
        std::string msg = "Scan results:\n";
        for(auto &ap: res.value()){
            char line[96]; snprintf(line, sizeof(line), " %s (%d dBm) ch %d\n", ap.ssid, ap.rssi, ap.channel);
            msg += line;
        }
        return CommandResult{0, CommandId::WIFI_SCAN, CommandStatus::SUCCESS, "wifi scan", msg, 0, 0};
    }});
    register_command({CommandId::WIFI_CONNECT, "wifi_connect", "Connect WiFi", "wifi connect <ssid> <pass>", [](auto args){
        if(args.size()<2) return CommandResult{0, CommandId::WIFI_CONNECT, CommandStatus::INVALID, "wifi connect", "Usage: wifi connect <ssid> <password>", 4002, 0};
        auto res = connectivity::WifiService::instance().connect(args[0], args[1]);
        if(res.is_ok()) return CommandResult{0, CommandId::WIFI_CONNECT, CommandStatus::SUCCESS, "wifi connect", "Connected to " + args[0], 0, 0};
        else return CommandResult{0, CommandId::WIFI_CONNECT, CommandStatus::FAILED, "wifi connect", res.error_message(), (int)res.error(), 0};
    }});

    // ble
    register_command({CommandId::BLE_STATUS, "ble_status", "BLE status", "ble status", [](auto args){
        auto st = connectivity::BleService::instance().status();
        char buf[128]; snprintf(buf, sizeof(buf), "BLE state: %s advertising=%d connected=%d name=%s",
            (st.state==connectivity::BleState::ADVERTISING?"ADVERTISING": (st.state==connectivity::BleState::CONNECTED?"CONNECTED":"IDLE")),
            st.advertising, st.connected, st.device_name);
        return CommandResult{0, CommandId::BLE_STATUS, CommandStatus::SUCCESS, "ble status", buf, 0, 0};
    }});
    register_command({CommandId::BLE_START, "ble_start", "Start BLE advertising", "ble start", [](auto args){
        auto r = connectivity::BleService::instance().start_advertising();
        if(r.is_ok()) return CommandResult{0, CommandId::BLE_START, CommandStatus::SUCCESS, "ble start", "BLE advertising started", 0, 0};
        return CommandResult{0, CommandId::BLE_START, CommandStatus::FAILED, "ble start", r.error_message(), (int)r.error(), 0};
    }});
    register_command({CommandId::BLE_STOP, "ble_stop", "Stop BLE", "ble stop", [](auto args){
        auto r = connectivity::BleService::instance().stop_advertising();
        return CommandResult{0, CommandId::BLE_STOP, CommandStatus::SUCCESS, "ble stop", "BLE stopped", 0, 0};
    }});

    // time
    register_command({CommandId::TIME_GET, "time_status", "Time status", "time status", [](auto args){
        auto st = time_service::TimeService::instance().status();
        struct tm tm{}; time_service::TimeService::instance().get_local_time(tm);
        char buf[128]; snprintf(buf, sizeof(buf), "Time synced=%d source=%d tz=%s %02d:%02d:%02d", st.synced, (int)st.source, st.timezone, tm.tm_hour, tm.tm_min, tm.tm_sec);
        return CommandResult{0, CommandId::TIME_GET, CommandStatus::SUCCESS, "time status", buf, 0, 0};
    }});
    register_command({CommandId::TIME_SYNC, "time_sync", "Sync time via SNTP", "time sync", [](auto args){
        auto r = time_service::TimeService::instance().start_sntp();
        if(r.is_ok()) return CommandResult{0, CommandId::TIME_SYNC, CommandStatus::SUCCESS, "time sync", "SNTP started, waiting sync", 0, 0};
        return CommandResult{0, CommandId::TIME_SYNC, CommandStatus::FAILED, "time sync", r.error_message(), (int)r.error(), 0};
    }});

    // display — uses 200ms finite lock, never infinite (R4)
    register_command({CommandId::DISPLAY_TEST, "display_test", "Display test pattern", "display test", [](auto args){
        auto& runtime = lvgl_adapter::LvglRuntime::instance();
        if (!runtime.lock(200)) return CommandResult{0, CommandId::DISPLAY_TEST, CommandStatus::BUSY, "display test", "Display busy (LVGL lock timeout 200ms)", 0, 0};
        auto r = display::GC9A01Display::instance().test_pattern();
        if(runtime.is_initialized()) {
            lv_obj_invalidate(lv_screen_active());
            lv_refr_now(runtime.display());
        }
        runtime.unlock();
        if(r.is_ok()) return CommandResult{0, CommandId::DISPLAY_TEST, CommandStatus::SUCCESS, "display test", "Test pattern OK", 0, 0};
        return CommandResult{0, CommandId::DISPLAY_TEST, CommandStatus::FAILED, "display test", r.error_message(), (int)r.error(), 0};
    }});
    register_command({CommandId::DISPLAY_BRIGHTNESS, "display_brightness", "Set brightness 0-100", "display brightness 75", [](auto args){
        if(args.empty()) return CommandResult{0, CommandId::DISPLAY_BRIGHTNESS, CommandStatus::INVALID, "display brightness", "Usage: display brightness <0-100>", 4002, 0};
        int v = atoi(args[0].c_str());
        if(v<0||v>100) return CommandResult{0, CommandId::DISPLAY_BRIGHTNESS, CommandStatus::INVALID, "display brightness", "Range 0-100", 4002, 0};
        display::GC9A01Display::instance().set_brightness(v);
        storage::SettingsStore::instance().settings().display_brightness = v;
        storage::SettingsStore::instance().save();
        char buf[32]; snprintf(buf, sizeof(buf), "Brightness %d%%", v);
        return CommandResult{0, CommandId::DISPLAY_BRIGHTNESS, CommandStatus::SUCCESS, "display brightness", buf, 0, 0};
    }});

    // diagnostics
    register_command({CommandId::SELF_TEST, "self-test", "Run self test", "self-test", [](auto args){
        auto res = diagnostics::Diagnostics::instance().run_self_test();
        std::string msg = "SELF TEST\n";
        for(int i=0;i<res.total;i++){
            msg += std::string(res.tests[i].name) + " " + (res.tests[i].pass?"PASS":"FAIL") + "\n";
        }
        char summary[64]; snprintf(summary, sizeof(summary), "%d TESTS %d PASSED %d FAILED RESULT: %s", res.total, res.passed, res.failed, res.overall_pass?"PASS":"FAIL");
        msg += summary;
        auto status = res.overall_pass ? CommandStatus::SUCCESS : CommandStatus::FAILED;
        return CommandResult{0, CommandId::SELF_TEST, status, "self-test", msg, 0, 0};
    }});
    register_command({CommandId::SELF_TEST, "self_test", "Run self test", "self_test", [](auto args){
        auto res = diagnostics::Diagnostics::instance().run_self_test();
        std::string msg;
        for(int i=0;i<res.total;i++) msg += std::string(res.tests[i].name) + (res.tests[i].pass?" PASS ":" FAIL ") + "\n";
        return CommandResult{0, CommandId::SELF_TEST, res.overall_pass?CommandStatus::SUCCESS:CommandStatus::FAILED, "self_test", msg, 0, 0};
    }});

    register_command({CommandId::REBOOT, "reboot", "Reboot device", "reboot", [](auto args){
        return CommandResult{0, CommandId::REBOOT, CommandStatus::SUCCESS, "reboot", "Rebooting...", 0, 0};
    }});
    register_command({CommandId::FACTORY_RESET, "factory_reset", "Factory reset NVS", "factory reset", [](auto args){
        storage::SettingsStore::instance().factory_reset();
        return CommandResult{0, CommandId::FACTORY_RESET, CommandStatus::SUCCESS, "factory reset", "Factory reset done", 0, 0};
    }});
    register_command({CommandId::RESET_INFO, "reset-info", "Show reset reason", "reset-info", [](auto args){
        auto reason = diagnostics::HealthMonitor::instance().reset_reason_str();
        return CommandResult{0, CommandId::RESET_INFO, CommandStatus::SUCCESS, "reset-info", std::string("Reset reason: ")+reason, 0, 0};
    }});

    // OTA
    register_command({CommandId::OTA_STATUS, "ota_status", "OTA status", "ota status", [](auto args){
        auto st = ota::OtaService::instance().status();
        char buf[128]; snprintf(buf, sizeof(buf), "OTA state=%d progress=%d%% error=%s", (int)st.state, st.progress_percent, st.error);
        return CommandResult{0, CommandId::OTA_STATUS, CommandStatus::SUCCESS, "ota status", buf, 0, 0};
    }});
    register_command({CommandId::OTA_UPDATE, "ota_update", "OTA update via HTTPS (non-blocking)", "ota update <https_url>", [](auto args){
        if(args.empty()) return CommandResult{0, CommandId::OTA_UPDATE, CommandStatus::INVALID, "ota update", "Usage: ota update https://example.com/firmware.bin", 4002, 0};
        auto r = ota::OtaService::instance().check_and_update_async(args[0], args.size()>=2?args[1]:"");
        if(r.is_ok()) return CommandResult{0, CommandId::OTA_UPDATE, CommandStatus::SUCCESS, "ota update", "OTA async started on Core0 (watchdog 30s), check ota_status", 0, 0};
        return CommandResult{0, CommandId::OTA_UPDATE, CommandStatus::FAILED, "ota update", r.error_message(), (int)r.error(), 0};
    }});

    // Microkernel Diagnostics
    register_command({CommandId::GET_STATUS, "kernel_status", "Show Nexos-RT status", "kernel status", [](auto args){
        mk_diagnostics_tick();
        auto kstats = mk_kernel_get_stats();
        char buf[320];
        char health[24];
        mk_diagnostics_copy_health(health, sizeof(health));
        snprintf(buf, sizeof(buf),
            "%s          v%s\n"
            "Status        %s\n"
            "Health        %s\n"
            "Uptime        %llu ms\n"
            "Tasks Active  %lu\n"
            "Ctx Switches  %llu\n"
            "Free Heap     %lu KB\n"
            "Min Heap      %lu KB\n",
            MK_CONFIG_OS_NAME,
            MK_CONFIG_VERSION_STRING,
            mk_is_running() ? "RUNNING" : "INITIALIZED",
            health,
            (unsigned long long)kstats.uptime_ms,
            (unsigned long)kstats.total_tasks,
            (unsigned long long)kstats.context_switches,
            (unsigned long)(kstats.free_heap / 1024),
            (unsigned long)(kstats.min_free_heap / 1024)
        );
        return CommandResult{0, CommandId::GET_STATUS, CommandStatus::SUCCESS, "kernel status", buf, 0, 0};
    }});

    register_command({CommandId::GET_STATUS, "kernel_tasks", "List Nexos-RT tasks", "kernel tasks", [](auto args){
        uint32_t count = mk_task_count();
        char buf[256];
        snprintf(buf, sizeof(buf), "Nexos-RT tasks active: %lu (Max: %d)\nScheduler: Preemptive Prio 0-%d\n",
                 (unsigned long)count, MK_CONFIG_MAX_TASKS, MK_CONFIG_MAX_PRIORITIES - 1);
        return CommandResult{0, CommandId::GET_STATUS, CommandStatus::SUCCESS, "kernel tasks", buf, 0, 0};
    }});

    register_command({CommandId::GET_STATUS, "kernel_stats", "Show Nexos-RT JSON stats", "kernel stats", [](auto args){
        mk_diagnostics_tick();
        auto kstats = mk_kernel_get_stats();
        char buf[320];
        char health[24];
        mk_diagnostics_copy_health(health, sizeof(health));
        snprintf(buf, sizeof(buf),
                 "{\"os\":\"%s\",\"version\":\"%s\",\"running\":%d,\"health\":\"%s\",\"uptime_ms\":%llu,\"tasks\":%lu,\"ctx_sw\":%llu,\"free_heap\":%lu}",
                 MK_CONFIG_OS_NAME, MK_CONFIG_VERSION_STRING, mk_is_running()?1:0,
                 health,
                 (unsigned long long)kstats.uptime_ms,
                 (unsigned long)kstats.total_tasks, (unsigned long long)kstats.context_switches, (unsigned long)kstats.free_heap);
        return CommandResult{0, CommandId::GET_STATUS, CommandStatus::SUCCESS, "kernel stats", buf, 0, 0};
    }});
}

} // namespace command
} // namespace smart_device

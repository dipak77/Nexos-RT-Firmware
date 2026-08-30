#include "command_service.h"
#include "common/app_version.h"
#include "common/string_utils.h"
#include "esp_log.h"
#include "esp_console.h"
#include "esp_system.h"
#include "argtable3/argtable3.h"
#include "mk.h"
#include "event_bus/event_bus.h"
#include "gui/gui_command.h"
#include "gui/gui.h"
#include <cstring>
#include <unistd.h>

static const char* TAG = "CMD_SERVICE";

namespace smart_device {
namespace command {

CommandService& CommandService::instance(){ static CommandService s; return s; }

Result<void> CommandService::initialize(){
    if(initialized_) return Result<void>::Ok();
    ESP_LOGI(TAG, "Initializing command service");

    CommandRegistry::instance().register_builtin_commands();

    // ESP console init
    esp_console_config_t console_cfg = ESP_CONSOLE_CONFIG_DEFAULT();
    console_cfg.max_cmdline_args = 16;
    console_cfg.hint_color = 0;
    esp_err_t ret = esp_console_init(&console_cfg);
    if(ret!=ESP_OK && ret!=ESP_ERR_INVALID_STATE){
        ESP_LOGE(TAG, "Console init failed: %s", esp_err_to_name(ret));
        return Result<void>::Err(AppError::SYSTEM_NOT_INITIALIZED, esp_err_to_name(ret));
    }

    // UART stdin is non-blocking on this board's primary/secondary console
    // VFS. The command task therefore uses a small polled line accumulator
    // instead of linenoise, whose ANSI probes and prompt retries assume a
    // blocking terminal transport.
    ESP_LOGI(TAG, "Using non-blocking polled UART line input");

    initialized_ = true;
    ESP_LOGI(TAG, "Command service initialized with %d commands", (int)CommandRegistry::instance().list_commands().size());
    return Result<void>::Ok();
}

CommandResult CommandService::execute_line(const std::string& line, const char* source){
    if(line.empty()) return CommandResult{0, CommandId::HELP, CommandStatus::INVALID, line, "Empty", 4002, 0};

    // Publish COMMAND_STARTED
    AppEvent ev_start{AppEventType::COMMAND_STARTED, 0, 0, ""};
    copy_cstr(ev_start.message, line.c_str());
    EventBus::instance().publish(ev_start);

    gui::CommandOverlay::instance().show_working(line.c_str());

    auto result = CommandRegistry::instance().execute(line);

    // Log result
    if(result.status==CommandStatus::SUCCESS){
        ESP_LOGI(TAG, "[CMD] id=%lu cmd=\"%s\" result=SUCCESS %lums", (unsigned long)result.id, result.command.c_str(), (unsigned long)result.execution_time_ms);
        ESP_LOGI(TAG, "%s", result.message.c_str());
        AppEvent ev{AppEventType::COMMAND_SUCCESS, result.id, (int32_t)result.execution_time_ms, ""};
        copy_cstr(ev.message, result.command.c_str());
        EventBus::instance().publish(ev);
        gui::CommandOverlay::instance().show_result(result.command.c_str(), result.status, ("✓ PASS\n"+result.message).c_str(), result.execution_time_ms);
    } else {
        ESP_LOGW(TAG, "[CMD] id=%lu cmd=\"%s\" result=%s code=%d %lums msg=%s",
                 (unsigned long)result.id, result.command.c_str(), command_status_to_string(result.status),
                 result.error_code, (unsigned long)result.execution_time_ms, result.message.c_str());
        AppEvent ev{AppEventType::COMMAND_FAILED, result.id, result.error_code, ""};
        copy_cstr(ev.message, result.message.c_str());
        EventBus::instance().publish(ev);
        char fail_msg[96]; snprintf(fail_msg, sizeof(fail_msg), "✕ FAIL\n%s\nERR %d", result.message.c_str(), result.error_code);
        gui::CommandOverlay::instance().show_result(result.command.c_str(), result.status, fail_msg, result.execution_time_ms);
    }

    // Handle special commands that need action
    if(result.command_id==CommandId::REBOOT && result.status==CommandStatus::SUCCESS){
        mk_sleep_ms(500);
        esp_restart();
    }

    return result;
}

static void console_task(void* arg){
    auto* svc = (CommandService*)arg;
    char line[256]{};
    size_t line_len = 0;
    bool swallow_lf = false;
    ESP_LOGI(TAG, "Console task started - type 'help'");
    printf("\n================================================\n");
    printf(" Smart Device Firmware\n");
    printf(" FW %s HW %s\n", APP_VERSION_STRING, APP_HW_VERSION);
    printf(" Type 'help' for commands\n");
    printf("================================================\n\n");
    printf("device> ");
    fflush(stdout);

    while(true){
        char c = 0;
        if(read(STDIN_FILENO, &c, 1) != 1) {
            // Non-blocking UART: yield while there is no input so the GUI,
            // radio stacks, and idle watchdog tasks continue to run.
            mk_watchdog_feed_self();
            mk_sleep_ms(20);
            continue;
        }

        if(c == '\n' && swallow_lf) {
            swallow_lf = false;
            continue;
        }

        if(c == '\r' || c == '\n') {
            swallow_lf = (c == '\r');
            printf("\r\n");
            if(line_len > 0) {
                line[line_len] = '\0';
                svc->execute_line(std::string(line), "UART");
                line_len = 0;
            }
            printf("device> ");
            fflush(stdout);
            continue;
        }

        swallow_lf = false;
        if(c == '\b' || static_cast<unsigned char>(c) == 0x7f) {
            if(line_len > 0) {
                --line_len;
                printf("\b \b");
                fflush(stdout);
            }
            continue;
        }

        if(c >= ' ' && c <= '~' && line_len < sizeof(line) - 1) {
            line[line_len++] = c;
            putchar(c);
            fflush(stdout);
        }
    }
}

Result<void> CommandService::start(){
    if(!initialized_) { auto r=initialize(); if(r.is_err()) return r; }
    if(running_) return Result<void>::Ok();
    running_ = true;
    ESP_LOGI(TAG, "Command service started");
    AppEvent ev{AppEventType::SYSTEM_BOOTED}; EventBus::instance().publish(ev);
    return Result<void>::Ok();
}

void CommandService::run_console(){
    console_task(this);
}

void CommandService::stop(){
    running_ = false;
}

} // namespace command
} // namespace smart_device

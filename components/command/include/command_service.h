#pragma once
#include "command_registry.h"
#include "common/result.h"

namespace smart_device {
namespace command {

class CommandService {
public:
    static CommandService& instance();
    Result<void> initialize();
    Result<void> start();
    void run_console();
    void stop();

    CommandResult execute_line(const std::string& line, const char* source = "UART");

    // Console integration
    void register_console_commands();

private:
    bool initialized_{false};
    bool running_{false};
};

} // namespace command
} // namespace smart_device

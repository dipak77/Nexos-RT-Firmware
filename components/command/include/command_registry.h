#pragma once
#include "command_result.h"
#include <functional>
#include <string>
#include <map>
#include <vector>

namespace smart_device {
namespace command {

using CommandHandler = std::function<CommandResult(const std::vector<std::string>& args)>;

struct CommandInfo {
    CommandId id;
    std::string name;
    std::string description;
    std::string usage;
    CommandHandler handler;
};

class CommandRegistry {
public:
    static CommandRegistry& instance();
    bool register_command(const CommandInfo& info);
    bool has_command(const std::string& name) const;
    CommandResult execute(const std::string& line);
    std::vector<CommandInfo> list_commands() const;
    void register_builtin_commands();

private:
    std::map<std::string, CommandInfo> commands_;
};

} // namespace command
} // namespace smart_device

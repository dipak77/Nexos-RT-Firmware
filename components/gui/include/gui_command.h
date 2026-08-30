#pragma once
#include "gui.h"

namespace smart_device {
namespace gui {
class CommandOverlay {
public:
    static CommandOverlay& instance();
    void show_working(const char* command);
    void show_result(const char* command, CommandStatus status, const char* message, uint32_t elapsed_ms);
    void hide();
};
} // namespace gui
} // namespace smart_device

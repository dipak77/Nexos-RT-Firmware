#include "gui_command.h"
#include "gui_dashboard.h"
namespace smart_device { namespace gui {
CommandOverlay& CommandOverlay::instance(){ static CommandOverlay s; return s; }
void CommandOverlay::show_working(const char* command){
    auto& g = Gui::instance();
    auto st = g.state();
    snprintf(st.latest_command, sizeof(st.latest_command), "%s", command);
    st.command_status = CommandStatus::BUSY;
    snprintf(st.command_message, sizeof(st.command_message), "WORKING...");
    g.update_state(st);
}
void CommandOverlay::show_result(const char* command, CommandStatus status, const char* message, uint32_t elapsed_ms){
    auto& g = Gui::instance();
    g.show_command_result(command, status, message, elapsed_ms);
}
void CommandOverlay::hide(){}
}} // namespace

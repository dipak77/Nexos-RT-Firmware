#include "gui_status.h"
namespace smart_device { namespace gui {
StatusOverlay& StatusOverlay::instance(){ static StatusOverlay s; return s; }
void StatusOverlay::show(const char* title, const char* msg, bool is_error){}
void StatusOverlay::hide(){}
}} // namespace

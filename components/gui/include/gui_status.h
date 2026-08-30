#pragma once
#include "lvgl.h"

namespace smart_device {
namespace gui {
class StatusOverlay {
public:
    static StatusOverlay& instance();
    void show(const char* title, const char* msg, bool is_error = false);
    void hide();
};
} // namespace gui
} // namespace smart_device

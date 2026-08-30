#pragma once
#include "mk/mk.h"

namespace smart_device {
namespace platform {

class Esp32S3Platform {
public:
    static Esp32S3Platform& instance();
    bool init();
    bool is_initialized() const { return initialized_; }
    mk_task_handle_t create_pinned_task(const char* name, mk_task_entry_t entry, void* arg, size_t stack, uint8_t prio, int core);

private:
    bool initialized_{false};
};

} // namespace platform
} // namespace smart_device

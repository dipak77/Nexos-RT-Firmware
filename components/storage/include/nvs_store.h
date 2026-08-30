#pragma once
#include "common/result.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <string>

namespace smart_device {
namespace storage {

class NvsStore {
public:
    static NvsStore& instance();
    Result<void> initialize();
    Result<void> erase_all();
    bool is_initialized() const { return initialized_; }

    Result<std::string> get_string(const char* key);
    Result<void> set_string(const char* key, const std::string& value);

    Result<int32_t> get_i32(const char* key);
    Result<void> set_i32(const char* key, int32_t value);

    Result<bool> get_bool(const char* key);
    Result<void> set_bool(const char* key, bool value);

private:
    bool initialized_{false};
};

} // namespace storage
} // namespace smart_device

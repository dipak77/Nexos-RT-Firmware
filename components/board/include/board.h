#pragma once
#include "board_config.h"
#include "common/result.h"

namespace smart_device {
namespace board {

class Board {
public:
    static Board& instance();
    Result<void> initialize(const BoardConfig& config = ESP32_S3_DEVKITC_V11);
    const BoardConfig& config() const { return config_; }
    bool is_initialized() const { return initialized_; }
    void print_info() const;
private:
    Board() = default;
    BoardConfig config_{};
    bool initialized_{false};
};

} // namespace board
} // namespace smart_device

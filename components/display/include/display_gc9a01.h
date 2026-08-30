#pragma once
#include "display.h"
#include "board/board_config.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_gc9a01.h"

namespace smart_device {
namespace display {

class GC9A01Display : public IDisplay {
public:
    static GC9A01Display& instance();
    Result<void> init() override;
    Result<void> clear(uint16_t color = 0x0000) override;
    Result<void> set_brightness(uint8_t percent) override;
    Result<void> test_pattern() override;
    bool is_initialized() const override { return initialized_; }

    esp_lcd_panel_handle_t panel_handle() const { return panel_handle_; }
    esp_lcd_panel_io_handle_t io_handle() const { return io_handle_; }

private:
    GC9A01Display() = default;
    bool initialized_{false};
    esp_lcd_panel_handle_t panel_handle_{nullptr};
    esp_lcd_panel_io_handle_t io_handle_{nullptr};
    board::BoardConfig config_{};
};

} // namespace display
} // namespace smart_device

#pragma once
#include "mk/mk.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "lvgl.h"
#include <atomic>

namespace smart_device {
namespace lvgl_adapter {

class LvglRuntime {
public:
    static LvglRuntime& instance();
    bool init(esp_lcd_panel_handle_t panel, esp_lcd_panel_io_handle_t io, int hres, int vres);
    void deinit();
    void tick(uint32_t ms);
    void lock();
    void unlock();
    uint32_t handle_timer(); // calls lv_timer_handler
    bool is_initialized() const { return initialized_; }
    lv_display_t* display() const { return lv_display_; }

private:
    bool initialized_{false};
    esp_lcd_panel_handle_t panel_{nullptr};
    esp_lcd_panel_io_handle_t io_{nullptr};
    lv_display_t* lv_display_{nullptr};
    mk_mutex_t* mutex_{nullptr};
    mk_timer_t* tick_timer_{nullptr};
    void* buf1_{nullptr};
    void* buf2_{nullptr};
    // LVGL can submit one transfer per draw buffer. With double buffering,
    // more than one SPI DMA completion can therefore be outstanding.
    std::atomic<uint32_t> pending_flush_count_{0};
    static void flush_cb(lv_display_t* disp, const lv_area_t* area, uint8_t* color_map);
    static bool color_trans_done_cb(esp_lcd_panel_io_handle_t panel_io,
                                    esp_lcd_panel_io_event_data_t* event_data,
                                    void* user_ctx);
};

} // namespace lvgl_adapter
} // namespace smart_device

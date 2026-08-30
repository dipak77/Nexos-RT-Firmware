#include "lvgl_adapter.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"

static const char* TAG = "LVGL_ADAPTER";

namespace smart_device {
namespace lvgl_adapter {

LvglRuntime& LvglRuntime::instance(){ static LvglRuntime s; return s; }

void LvglRuntime::flush_cb(lv_display_t* disp, const lv_area_t* area, uint8_t* color_map){
    auto* runtime = static_cast<LvglRuntime*>(lv_display_get_user_data(disp));
    if (!runtime || !runtime->panel_) {
        lv_display_flush_ready(disp);
        return;
    }
    const int x1 = area->x1;
    const int y1 = area->y1;
    const int x2 = area->x2 + 1;
    const int y2 = area->y2 + 1;

    // LVGL stores RGB565 words in CPU (little-endian) byte order, while the
    // SPI LCD consumes each RGB565 pixel most-significant byte first.
    lv_draw_sw_rgb565_swap(color_map, static_cast<uint32_t>((x2 - x1) * (y2 - y1)));

    // esp_lcd queues the DMA transaction. The draw buffer remains owned by the
    // driver until color_trans_done_cb acknowledges this specific transfer.
    // Count transactions rather than storing one display pointer because LVGL
    // can have both draw buffers in flight at the same time.
    runtime->pending_flush_count_.fetch_add(1, std::memory_order_acq_rel);
    esp_err_t ret = esp_lcd_panel_draw_bitmap(runtime->panel_, x1, y1, x2, y2, color_map);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "draw_bitmap failed: %s", esp_err_to_name(ret));
        uint32_t pending = runtime->pending_flush_count_.load(std::memory_order_acquire);
        while (pending > 0 &&
               !runtime->pending_flush_count_.compare_exchange_weak(
                   pending, pending - 1, std::memory_order_acq_rel)) {
        }
        lv_display_flush_ready(disp);
    }
}

bool LvglRuntime::color_trans_done_cb(esp_lcd_panel_io_handle_t panel_io,
                                      esp_lcd_panel_io_event_data_t* event_data,
                                      void* user_ctx){
    (void)panel_io;
    (void)event_data;
    auto* runtime = static_cast<LvglRuntime*>(user_ctx);
    if (!runtime) return false;

    uint32_t pending = runtime->pending_flush_count_.load(std::memory_order_acquire);
    while (pending > 0 &&
           !runtime->pending_flush_count_.compare_exchange_weak(
               pending, pending - 1, std::memory_order_acq_rel)) {
    }
    if (pending > 0 && runtime->lv_display_) {
        // One esp_lcd completion corresponds to exactly one LVGL flush.
        lv_display_flush_ready(runtime->lv_display_);
    }
    return false;
}

bool LvglRuntime::init(esp_lcd_panel_handle_t panel, esp_lcd_panel_io_handle_t io, int hres, int vres){
    if(initialized_) return true;
    panel_=panel; io_=io;

    ESP_LOGI(TAG, "Initializing LVGL 9.5 via microkernel (direct adapter)");

    lv_init();
    mutex_ = mk_mutex_create("lvgl_mutex");
    if(!mutex_){ ESP_LOGE(TAG, "Mutex create failed"); return false; }

    // 40 lines of RGB565 (~19 KB x2). Fits in DMA internal RAM on ESP32-S3.
    const int lines = 40;
    size_t buf_size = static_cast<size_t>(hres) * lines * sizeof(uint16_t);
    buf1_ = heap_caps_malloc(buf_size, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    buf2_ = heap_caps_malloc(buf_size, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if(!buf1_){
        buf1_ = heap_caps_malloc(buf_size, MALLOC_CAP_DMA);
    }
    if(!buf1_){
        ESP_LOGE(TAG, "Buf alloc failed (%u bytes)", (unsigned)buf_size);
        return false;
    }
    if(!buf2_){
        ESP_LOGW(TAG, "Second DMA buf failed, using single buffer");
    }

    lv_display_ = lv_display_create(hres, vres);
    if(!lv_display_){
        ESP_LOGE(TAG, "lv_display_create failed");
        return false;
    }
    lv_display_set_user_data(lv_display_, this);
    lv_display_set_flush_cb(lv_display_, flush_cb);
    lv_display_set_buffers(lv_display_, buf1_, buf2_, buf_size, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_color_format(lv_display_, LV_COLOR_FORMAT_RGB565);

    const esp_lcd_panel_io_callbacks_t io_callbacks = {
        .on_color_trans_done = color_trans_done_cb,
    };
    esp_err_t callback_ret = esp_lcd_panel_io_register_event_callbacks(io_, &io_callbacks, this);
    if (callback_ret != ESP_OK) {
        ESP_LOGE(TAG, "Panel DMA callback registration failed: %s", esp_err_to_name(callback_ret));
        return false;
    }

    // LVGL tick is advanced from the GUI thread using mk_time_ms().
    tick_timer_ = nullptr;

    initialized_ = true;
    ESP_LOGI(TAG, "LVGL adapter init OK %dx%d buf=%u double=%d", hres, vres, (unsigned)buf_size, buf2_ != nullptr);
    return true;
}

void LvglRuntime::tick(uint32_t ms){
    if (ms) lv_tick_inc(ms);
}

void LvglRuntime::lock(){
    if(mutex_) mk_mutex_lock(mutex_, 0xFFFFFFFF);
}
void LvglRuntime::unlock(){
    if(mutex_) mk_mutex_unlock(mutex_);
}

uint32_t LvglRuntime::handle_timer(){
    lock();
    uint32_t delay = lv_timer_handler();
    unlock();
    return delay;
}

void LvglRuntime::deinit(){
    if(tick_timer_) mk_timer_stop(tick_timer_, 0);
    pending_flush_count_.store(0, std::memory_order_release);
    initialized_=false;
}

} // namespace lvgl_adapter
} // namespace smart_device

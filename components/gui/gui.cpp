#include "gui.h"
#include "common/string_utils.h"
#include "gui_dashboard.h"
#include "board/board.h"
#include "lvgl_adapter/lvgl_adapter.h"
#include "esp_log.h"
#include "lvgl.h"
#include "common/app_version.h"
#include "mk.h"
#include <cstring>

static const char* TAG = "GUI";

namespace smart_device {
namespace gui {

Gui& Gui::instance(){ static Gui s; return s; }

Result<void> Gui::initialize(esp_lcd_panel_handle_t panel, esp_lcd_panel_io_handle_t io){
    if(initialized_) return Result<void>::Ok();
    ESP_LOGI(TAG, "Initializing GUI on LVGL 9.5 direct adapter (%s)", MK_CONFIG_OS_NAME);

    auto& runtime = lvgl_adapter::LvglRuntime::instance();
    auto& board_cfg = board::Board::instance().config();
    if (!runtime.is_initialized()) {
        bool ok = runtime.init(panel, io, board_cfg.lcd_hres, board_cfg.lcd_vres);
        if (!ok) {
            return Result<void>::Err(AppError::DISPLAY_INIT_FAILED, "lvgl adapter");
        }
    }

    copy_cstr(ui_state_.firmware_version, APP_VERSION_STRING);

    runtime.lock();
    lv_obj_t* scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x0A0A0A), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    DashboardScreen::instance().create(scr);
    lv_refr_now(runtime.display());
    runtime.unlock();

    initialized_ = true;
    ESP_LOGI(TAG, "GUI initialized %dx%d", board_cfg.lcd_hres, board_cfg.lcd_vres);
    return Result<void>::Ok();
}

Result<void> Gui::start(){
    if(!initialized_) return Result<void>::Err(AppError::DISPLAY_NOT_INITIALIZED, "GUI not init");
    ESP_LOGI(TAG, "GUI running via %s GUI thread", MK_CONFIG_OS_NAME);
    return Result<void>::Ok();
}

void Gui::update_state(const UiState& state){
    ui_state_ = state;
    if(!initialized_) return;
    auto& runtime = lvgl_adapter::LvglRuntime::instance();
    if(runtime.lock(200)){
        DashboardScreen::instance().update(ui_state_);
        runtime.unlock();
    } else {
        ESP_LOGW(TAG, "LVGL lock timeout in update_state — skipping frame");
    }
}

void Gui::show_command_result(const char* cmd, CommandStatus status, const char* msg, uint32_t time_ms){
    copy_cstr(ui_state_.latest_command, cmd);
    ui_state_.command_status = status;
    copy_cstr(ui_state_.command_message, msg);
    ui_state_.command_time_ms = time_ms;
    update_state(ui_state_);
}

void Gui::show_splash(bool show){
    ui_state_.show_splash = show;
}

} // namespace gui
} // namespace smart_device

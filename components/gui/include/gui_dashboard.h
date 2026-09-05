#pragma once
#include "gui.h"
#include "lvgl.h"

namespace smart_device {
namespace gui {

class DashboardScreen {
public:
    static DashboardScreen& instance();
    void create(lv_obj_t* parent);
    void update(const UiState& state);
    void destroy();
    void play_boot_animation();
    void play_command_success();
    void play_command_fail();

    lv_obj_t* root{nullptr};

private:
    // Premium glassmorphism visual elements
    lv_obj_t* bg_gradient{nullptr};
    lv_obj_t* outer_glow_arc{nullptr};
    lv_obj_t* inner_ring{nullptr};
    
    // Top status pills
    lv_obj_t* wifi_pill{nullptr};
    lv_obj_t* wifi_dot{nullptr};
    lv_obj_t* wifi_label{nullptr};
    lv_obj_t* ble_pill{nullptr};
    lv_obj_t* ble_dot{nullptr};
    lv_obj_t* ble_label{nullptr};
    
    // Time cluster
    lv_obj_t* time_container{nullptr};
    lv_obj_t* time_label{nullptr};
    lv_obj_t* time_sec_label{nullptr};
    lv_obj_t* date_label{nullptr};
    lv_obj_t* ampm_label{nullptr};
    
    // Divider with glow
    lv_obj_t* divider{nullptr};
    
    // System status chip
    lv_obj_t* status_chip{nullptr};
    lv_obj_t* status_dot{nullptr};
    lv_obj_t* status_label{nullptr};
    
    // Command glass card
    lv_obj_t* cmd_card{nullptr};
    lv_obj_t* cmd_icon{nullptr};
    lv_obj_t* cmd_label{nullptr};
    lv_obj_t* cmd_result_label{nullptr};
    lv_obj_t* cmd_time_label{nullptr};
    
    // Bottom info
    lv_obj_t* fw_label{nullptr};
    lv_obj_t* uptime_label{nullptr};
    lv_obj_t* heap_bar{nullptr};
    lv_obj_t* heap_fill{nullptr};
    
    bool created_{false};
};

} // namespace gui
} // namespace smart_device

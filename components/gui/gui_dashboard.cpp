#include "gui_dashboard.h"
#include "gui_theme.h"
#include "common/app_version.h"
#include "esp_log.h"
#include <cstdio>
#include <cstring>

static const char* TAG = "DASHBOARD_PREMIUM";

namespace smart_device {
namespace gui {

DashboardScreen& DashboardScreen::instance(){ static DashboardScreen s; return s; }

static void anim_cb_set_arc_value(void* var, int32_t v){
    lv_arc_set_value((lv_obj_t*)var, v);
}

void DashboardScreen::create(lv_obj_t* parent){
    if(created_) return;
    
    using namespace theme;
    
    // Root - perfect circle 240x240
    root = lv_obj_create(parent);
    lv_obj_set_size(root, 240, 240);
    lv_obj_center(root);
    lv_obj_set_style_radius(root, 120, 0);
    lv_obj_set_style_bg_color(root, c(COLOR_BG_DEEP), 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(root, 0, 0);
    lv_obj_set_style_pad_all(root, 0, 0);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_clip_corner(root, true, 0);
    
    // Background subtle gradient
    bg_gradient = lv_obj_create(root);
    lv_obj_set_size(bg_gradient, 240, 240);
    lv_obj_center(bg_gradient);
    lv_obj_set_style_radius(bg_gradient, 120, 0);
    lv_obj_set_style_bg_color(bg_gradient, c(0x12141C), 0);
    lv_obj_set_style_bg_grad_color(bg_gradient, c(COLOR_BG_DEEP), 0);
    lv_obj_set_style_bg_grad_dir(bg_gradient, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_border_width(bg_gradient, 0, 0);
    lv_obj_clear_flag(bg_gradient, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(bg_gradient, 0, 0);
    
    // Outer glow arc - premium cyan ring
    outer_glow_arc = lv_arc_create(root);
    lv_obj_set_size(outer_glow_arc, 232, 232);
    lv_obj_center(outer_glow_arc);
    lv_arc_set_range(outer_glow_arc, 0, 100);
    lv_arc_set_value(outer_glow_arc, 85);
    lv_arc_set_bg_angles(outer_glow_arc, 0, 360);
    lv_arc_set_angles(outer_glow_arc, 45, 315); // 270 deg open for modern look
    lv_obj_set_style_arc_color(outer_glow_arc, c(0x1E222E), LV_PART_MAIN);
    lv_obj_set_style_arc_width(outer_glow_arc, 1, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(outer_glow_arc, LV_OPA_50, LV_PART_MAIN);
    lv_obj_set_style_arc_color(outer_glow_arc, c(COLOR_ACCENT_CYAN), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(outer_glow_arc, 2, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(outer_glow_arc, true, LV_PART_INDICATOR);
    lv_obj_remove_style(outer_glow_arc, nullptr, LV_PART_KNOB);
    lv_obj_clear_flag(outer_glow_arc, LV_OBJ_FLAG_CLICKABLE);
    
    // Inner thin ring
    inner_ring = lv_obj_create(root);
    lv_obj_set_size(inner_ring, 216, 216);
    lv_obj_center(inner_ring);
    lv_obj_set_style_radius(inner_ring, 108, 0);
    lv_obj_set_style_bg_opa(inner_ring, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(inner_ring, 1, 0);
    lv_obj_set_style_border_color(inner_ring, c(COLOR_BORDER), 0);
    lv_obj_set_style_border_opa(inner_ring, LV_OPA_20, 0);
    lv_obj_clear_flag(inner_ring, LV_OBJ_FLAG_SCROLLABLE);
    
    // === TOP STATUS PILLS ===
    // WiFi pill - glassmorphic
    wifi_pill = lv_obj_create(root);
    lv_obj_set_size(wifi_pill, 62, 20);
    lv_obj_align(wifi_pill, LV_ALIGN_TOP_MID, -36, 26);
    lv_obj_set_style_radius(wifi_pill, 10, 0);
    lv_obj_set_style_bg_color(wifi_pill, c(0x1E222E), 0);
    lv_obj_set_style_bg_opa(wifi_pill, LV_OPA_70, 0);
    lv_obj_set_style_border_width(wifi_pill, 1, 0);
    lv_obj_set_style_border_color(wifi_pill, c(COLOR_BORDER), 0);
    lv_obj_set_style_border_opa(wifi_pill, LV_OPA_30, 0);
    lv_obj_set_style_pad_all(wifi_pill, 0, 0);
    lv_obj_clear_flag(wifi_pill, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(wifi_pill, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(wifi_pill, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(wifi_pill, 4, 0);
    
    wifi_dot = lv_obj_create(wifi_pill);
    lv_obj_set_size(wifi_dot, 6, 6);
    lv_obj_set_style_radius(wifi_dot, 3, 0);
    lv_obj_set_style_bg_color(wifi_dot, c(COLOR_WIFI_BAD), 0);
    lv_obj_set_style_border_width(wifi_dot, 0, 0);
    lv_obj_set_style_shadow_width(wifi_dot, 8, 0);
    lv_obj_set_style_shadow_color(wifi_dot, c(COLOR_WIFI_OK), 0);
    lv_obj_set_style_shadow_opa(wifi_dot, LV_OPA_60, 0);
    
    wifi_label = lv_label_create(wifi_pill);
    lv_label_set_text(wifi_label, "WiFi");
    lv_obj_set_style_text_color(wifi_label, c(COLOR_TEXT_SECONDARY), 0);
    lv_obj_set_style_text_font(wifi_label, &lv_font_montserrat_10, 0);
    
    // BLE pill
    ble_pill = lv_obj_create(root);
    lv_obj_set_size(ble_pill, 62, 20);
    lv_obj_align(ble_pill, LV_ALIGN_TOP_MID, 36, 26);
    lv_obj_set_style_radius(ble_pill, 10, 0);
    lv_obj_set_style_bg_color(ble_pill, c(0x1E222E), 0);
    lv_obj_set_style_bg_opa(ble_pill, LV_OPA_70, 0);
    lv_obj_set_style_border_width(ble_pill, 1, 0);
    lv_obj_set_style_border_color(ble_pill, c(COLOR_BORDER), 0);
    lv_obj_set_style_border_opa(ble_pill, LV_OPA_30, 0);
    lv_obj_set_style_pad_all(ble_pill, 0, 0);
    lv_obj_clear_flag(ble_pill, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(ble_pill, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ble_pill, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(ble_pill, 4, 0);
    
    ble_label = lv_label_create(ble_pill);
    lv_label_set_text(ble_label, "BLE");
    lv_obj_set_style_text_color(ble_label, c(COLOR_TEXT_SECONDARY), 0);
    lv_obj_set_style_text_font(ble_label, &lv_font_montserrat_10, 0);
    
    ble_dot = lv_obj_create(ble_pill);
    lv_obj_set_size(ble_dot, 6, 6);
    lv_obj_set_style_radius(ble_dot, 3, 0);
    lv_obj_set_style_bg_color(ble_dot, c(COLOR_WIFI_BAD), 0);
    lv_obj_set_style_border_width(ble_dot, 0, 0);
    
    // === TIME CLUSTER ===
    time_container = lv_obj_create(root);
    lv_obj_set_size(time_container, 180, 44);
    lv_obj_align(time_container, LV_ALIGN_TOP_MID, 0, 48);
    lv_obj_set_style_bg_opa(time_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(time_container, 0, 0);
    lv_obj_clear_flag(time_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(time_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(time_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(time_container, 0, 0);
    
    // Time row: HH:MM + seconds
    lv_obj_t* time_row = lv_obj_create(time_container);
    lv_obj_set_size(time_row, 180, 28);
    lv_obj_set_style_bg_opa(time_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(time_row, 0, 0);
    lv_obj_set_style_pad_all(time_row, 0, 0);
    lv_obj_clear_flag(time_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(time_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(time_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER);
    
    time_label = lv_label_create(time_row);
    lv_label_set_text(time_label, "12:24");
    lv_obj_set_style_text_color(time_label, c(COLOR_TEXT_PRIMARY), 0);
    lv_obj_set_style_text_font(time_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_letter_space(time_label, -1, 0);
    
    time_sec_label = lv_label_create(time_row);
    lv_label_set_text(time_sec_label, ":38");
    lv_obj_set_style_text_color(time_sec_label, c(COLOR_TEXT_DIM), 0);
    lv_obj_set_style_text_font(time_sec_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_pad_left(time_sec_label, 2, 0);
    
    ampm_label = lv_label_create(time_container);
    lv_label_set_text(ampm_label, "");
    lv_obj_set_style_text_color(ampm_label, c(COLOR_ACCENT_CYAN), 0);
    lv_obj_set_style_text_font(ampm_label, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_letter_space(ampm_label, 2, 0);
    
    // Date - uppercase tracking
    date_label = lv_label_create(root);
    lv_obj_align(date_label, LV_ALIGN_TOP_MID, 0, 92);
    lv_label_set_text(date_label, "28 AUG 2026");
    lv_obj_set_style_text_color(date_label, c(COLOR_ACCENT_CYAN), 0);
    lv_obj_set_style_text_font(date_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_letter_space(date_label, 1, 0);
    
    // === DIVIDER WITH GLOW ===
    divider = lv_obj_create(root);
    lv_obj_set_size(divider, 80, 1);
    lv_obj_align(divider, LV_ALIGN_CENTER, 0, -12);
    lv_obj_set_style_bg_color(divider, c(COLOR_DIVIDER), 0);
    lv_obj_set_style_bg_grad_color(divider, c(COLOR_ACCENT_CYAN), 0);
    lv_obj_set_style_bg_grad_dir(divider, LV_GRAD_DIR_HOR, 0);
    lv_obj_set_style_border_width(divider, 0, 0);
    
    // === SYSTEM STATUS CHIP ===
    status_chip = lv_obj_create(root);
    lv_obj_set_size(status_chip, 96, 20);
    lv_obj_align(status_chip, LV_ALIGN_CENTER, 0, 4);
    lv_obj_set_style_radius(status_chip, 10, 0);
    lv_obj_set_style_bg_color(status_chip, c(0x0F2E1F), 0);
    lv_obj_set_style_bg_opa(status_chip, LV_OPA_80, 0);
    lv_obj_set_style_border_width(status_chip, 1, 0);
    lv_obj_set_style_border_color(status_chip, c(COLOR_ACCENT_GREEN), 0);
    lv_obj_set_style_border_opa(status_chip, LV_OPA_30, 0);
    lv_obj_clear_flag(status_chip, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(status_chip, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(status_chip, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(status_chip, 6, 0);
    lv_obj_set_style_pad_all(status_chip, 0, 0);
    
    status_dot = lv_obj_create(status_chip);
    lv_obj_set_size(status_dot, 6, 6);
    lv_obj_set_style_radius(status_dot, 3, 0);
    lv_obj_set_style_bg_color(status_dot, c(COLOR_ACCENT_GREEN), 0);
    lv_obj_set_style_border_width(status_dot, 0, 0);
    lv_obj_set_style_shadow_width(status_dot, 10, 0);
    lv_obj_set_style_shadow_color(status_dot, c(COLOR_ACCENT_GREEN), 0);
    lv_obj_set_style_shadow_opa(status_dot, LV_OPA_70, 0);
    
    status_label = lv_label_create(status_chip);
    lv_label_set_text(status_label, "SYSTEM OK");
    lv_obj_set_style_text_color(status_label, c(COLOR_ACCENT_GREEN), 0);
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_letter_space(status_label, 1, 0);
    
    // === COMMAND GLASS CARD ===
    cmd_card = lv_obj_create(root);
    lv_obj_set_size(cmd_card, 176, 56);
    lv_obj_align(cmd_card, LV_ALIGN_CENTER, 0, 42);
    apply_glass_card(cmd_card);
    lv_obj_set_style_pad_all(cmd_card, 6, 0);
    lv_obj_set_flex_flow(cmd_card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cmd_card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(cmd_card, 2, 0);
    
    // Top row: icon + cmd name
    lv_obj_t* cmd_top = lv_obj_create(cmd_card);
    lv_obj_set_size(cmd_top, 164, 16);
    lv_obj_set_style_bg_opa(cmd_top, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cmd_top, 0, 0);
    lv_obj_set_style_pad_all(cmd_top, 0, 0);
    lv_obj_clear_flag(cmd_top, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(cmd_top, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cmd_top, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    cmd_label = lv_label_create(cmd_top);
    lv_label_set_text(cmd_label, "CMD: WIFI STATUS");
    lv_obj_set_style_text_color(cmd_label, c(COLOR_TEXT_SECONDARY), 0);
    lv_obj_set_style_text_font(cmd_label, &lv_font_montserrat_10, 0);
    lv_label_set_long_mode(cmd_label, LV_LABEL_LONG_DOT);
    lv_obj_set_width(cmd_label, 122);
    
    cmd_time_label = lv_label_create(cmd_top);
    lv_label_set_text(cmd_time_label, "5 ms");
    lv_obj_set_style_text_color(cmd_time_label, c(COLOR_TEXT_DIM), 0);
    lv_obj_set_style_text_font(cmd_time_label, &lv_font_montserrat_10, 0);
    lv_obj_set_width(cmd_time_label, 38);
    lv_obj_set_style_text_align(cmd_time_label, LV_TEXT_ALIGN_RIGHT, 0);
    
    // Bottom row: result
    lv_obj_t* cmd_bottom = lv_obj_create(cmd_card);
    lv_obj_set_size(cmd_bottom, 164, 20);
    lv_obj_set_style_bg_opa(cmd_bottom, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cmd_bottom, 0, 0);
    lv_obj_set_style_pad_all(cmd_bottom, 0, 0);
    lv_obj_clear_flag(cmd_bottom, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(cmd_bottom, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cmd_bottom, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(cmd_bottom, 6, 0);
    
    cmd_icon = lv_label_create(cmd_bottom);
    lv_label_set_text(cmd_icon, LV_SYMBOL_OK);
    lv_obj_set_style_text_color(cmd_icon, c(COLOR_ACCENT_GREEN), 0);
    lv_obj_set_style_text_font(cmd_icon, &lv_font_montserrat_12, 0);
    
    cmd_result_label = lv_label_create(cmd_bottom);
    lv_label_set_text(cmd_result_label, "PASS • -54 dBm");
    lv_obj_set_style_text_color(cmd_result_label, c(COLOR_TEXT_PRIMARY), 0);
    lv_obj_set_style_text_font(cmd_result_label, &lv_font_montserrat_12, 0);
    lv_label_set_long_mode(cmd_result_label, LV_LABEL_LONG_DOT);
    lv_obj_set_width(cmd_result_label, 140);
    
    // === BOTTOM INFO ===
    fw_label = lv_label_create(root);
    lv_obj_align(fw_label, LV_ALIGN_BOTTOM_MID, -48, -24);
    lv_label_set_text(fw_label, "FW v1.2.0");
    lv_obj_set_style_text_color(fw_label, c(COLOR_TEXT_DIM), 0);
    lv_obj_set_style_text_font(fw_label, &lv_font_montserrat_10, 0);
    
    uptime_label = lv_label_create(root);
    lv_obj_align(uptime_label, LV_ALIGN_BOTTOM_MID, 48, -24);
    lv_label_set_text(uptime_label, "00:12:38");
    lv_obj_set_style_text_color(uptime_label, c(COLOR_TEXT_DIM), 0);
    lv_obj_set_style_text_font(uptime_label, &lv_font_montserrat_10, 0);
    
    // Heap bar
    heap_bar = lv_obj_create(root);
    lv_obj_set_size(heap_bar, 40, 2);
    lv_obj_align(heap_bar, LV_ALIGN_BOTTOM_MID, 0, -14);
    lv_obj_set_style_radius(heap_bar, 1, 0);
    lv_obj_set_style_bg_color(heap_bar, c(COLOR_DIVIDER), 0);
    lv_obj_set_style_bg_opa(heap_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(heap_bar, 0, 0);
    
    heap_fill = lv_obj_create(heap_bar);
    lv_obj_set_size(heap_fill, 28, 2);
    lv_obj_set_style_radius(heap_fill, 1, 0);
    lv_obj_set_style_bg_color(heap_fill, c(COLOR_ACCENT_CYAN), 0);
    lv_obj_set_style_border_width(heap_fill, 0, 0);
    
    created_ = true;
    ESP_LOGI(TAG, "Premium dashboard created - glassmorphism + animations");
    
    play_boot_animation();
}

void DashboardScreen::update(const UiState& state){
    if(!created_) return;
    using namespace theme;
    
    // Time with seconds & dynamic AM/PM
    if(time_label) lv_label_set_text(time_label, state.time_str);
    if(date_label) lv_label_set_text(date_label, state.date_str);
    if(time_sec_label){
        lv_label_set_text(time_sec_label, state.time_full[0] ? state.time_full + 5 : ":00");
    }
    if(ampm_label){
        if(state.ampm_str[0] != '\0'){
            lv_obj_clear_flag(ampm_label, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(ampm_label, state.ampm_str);
        } else {
            lv_obj_add_flag(ampm_label, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(ampm_label, "");
        }
    }
    
    // WiFi pill
    if(wifi_dot && wifi_label && wifi_pill){
        if(state.wifi_connected){
            lv_obj_set_style_bg_color(wifi_dot, c(COLOR_WIFI_OK), 0);
            lv_obj_set_style_shadow_color(wifi_dot, c(COLOR_WIFI_OK), 0);
            lv_obj_set_style_shadow_opa(wifi_dot, LV_OPA_60, 0);
            lv_obj_set_style_border_color(wifi_pill, c(COLOR_WIFI_OK), 0);
            lv_obj_set_style_border_opa(wifi_pill, LV_OPA_20, 0);
            char txt[24]; snprintf(txt, sizeof(txt), "WiFi %d", state.wifi_rssi);
            lv_label_set_text(wifi_label, txt);
            lv_obj_set_style_text_color(wifi_label, c(COLOR_TEXT_PRIMARY), 0);
        } else if(state.wifi_ap_running){
            lv_obj_set_style_bg_color(wifi_dot, c(COLOR_BLE_ADV), 0);
            lv_obj_set_style_shadow_opa(wifi_dot, LV_OPA_40, 0);
            lv_obj_set_style_border_color(wifi_pill, c(COLOR_BLE_ADV), 0);
            lv_obj_set_style_border_opa(wifi_pill, LV_OPA_20, 0);
            lv_label_set_text(wifi_label, "AP ON");
            lv_obj_set_style_text_color(wifi_label, c(COLOR_TEXT_PRIMARY), 0);
        } else {
            lv_obj_set_style_bg_color(wifi_dot, c(COLOR_WIFI_BAD), 0);
            lv_obj_set_style_shadow_opa(wifi_dot, LV_OPA_0, 0);
            lv_obj_set_style_border_opa(wifi_pill, LV_OPA_10, 0);
            lv_label_set_text(wifi_label, "WiFi --");
            lv_obj_set_style_text_color(wifi_label, c(COLOR_TEXT_SECONDARY), 0);
        }
    }
    
    // BLE
    if(ble_dot && ble_label && ble_pill){
        if(state.ble_connected){
            lv_obj_set_style_bg_color(ble_dot, c(COLOR_BLE_CONN), 0);
            lv_label_set_text(ble_label, "BLE •");
            lv_obj_set_style_border_color(ble_pill, c(COLOR_BLE_CONN), 0);
        } else if(state.ble_advertising){
            lv_obj_set_style_bg_color(ble_dot, c(COLOR_BLE_ADV), 0);
            lv_label_set_text(ble_label, "BLE");
            lv_obj_set_style_border_color(ble_pill, c(COLOR_BLE_ADV), 0);
        } else {
            lv_obj_set_style_bg_color(ble_dot, c(COLOR_WIFI_BAD), 0);
            lv_label_set_text(ble_label, "BLE");
        }
    }
    
    // System status chip
    if(status_label && status_chip && status_dot){
        lv_label_set_text(status_label, state.system_status);
        if(strstr(state.system_status, "OK")){
            lv_obj_set_style_bg_color(status_chip, c(0x0F2E1F), 0);
            lv_obj_set_style_border_color(status_chip, c(COLOR_ACCENT_GREEN), 0);
            lv_obj_set_style_text_color(status_label, c(COLOR_ACCENT_GREEN), 0);
            lv_obj_set_style_bg_color(status_dot, c(COLOR_ACCENT_GREEN), 0);
        } else if(strstr(state.system_status, "SYNC")){
            lv_obj_set_style_bg_color(status_chip, c(0x2E2410), 0);
            lv_obj_set_style_border_color(status_chip, c(COLOR_ACCENT_ORANGE), 0);
            lv_obj_set_style_text_color(status_label, c(COLOR_ACCENT_ORANGE), 0);
            lv_obj_set_style_bg_color(status_dot, c(COLOR_ACCENT_ORANGE), 0);
        } else {
            lv_obj_set_style_bg_color(status_chip, c(0x2E1515), 0);
            lv_obj_set_style_border_color(status_chip, c(COLOR_ACCENT_RED), 0);
            lv_obj_set_style_text_color(status_label, c(COLOR_ACCENT_RED), 0);
        }
    }
    
    // Command card
    if(cmd_label){
        char txt[48]; snprintf(txt, sizeof(txt), "CMD: %s", state.latest_command);
        lv_label_set_text(cmd_label, txt);
    }
    if(cmd_result_label && cmd_icon && cmd_time_label){
        lv_label_set_text(cmd_result_label, state.command_message);
        char t_buf[16];
        if(state.command_time_ms > 0){
            snprintf(t_buf, sizeof(t_buf), "%lu ms", (unsigned long)state.command_time_ms);
        } else {
            snprintf(t_buf, sizeof(t_buf), "<1 ms");
        }
        lv_label_set_text(cmd_time_label, t_buf);
        if(state.command_status==CommandStatus::SUCCESS){
            lv_label_set_text(cmd_icon, LV_SYMBOL_OK);
            lv_obj_set_style_text_color(cmd_icon, c(COLOR_ACCENT_GREEN), 0);
            lv_obj_set_style_text_color(cmd_result_label, c(COLOR_TEXT_PRIMARY), 0);
            lv_obj_set_style_border_color(cmd_card, c(COLOR_ACCENT_GREEN), 0);
            lv_obj_set_style_border_opa(cmd_card, LV_OPA_20, 0);
        } else if(state.command_status==CommandStatus::FAILED){
            lv_label_set_text(cmd_icon, LV_SYMBOL_CLOSE);
            lv_obj_set_style_text_color(cmd_icon, c(COLOR_ACCENT_RED), 0);
            lv_obj_set_style_text_color(cmd_result_label, c(COLOR_ACCENT_RED), 0);
            lv_obj_set_style_border_color(cmd_card, c(COLOR_ACCENT_RED), 0);
            lv_obj_set_style_border_opa(cmd_card, LV_OPA_30, 0);
        } else {
            lv_label_set_text(cmd_icon, LV_SYMBOL_REFRESH);
            lv_obj_set_style_text_color(cmd_icon, c(COLOR_ACCENT_ORANGE), 0);
            lv_obj_set_style_text_color(cmd_result_label, c(COLOR_ACCENT_ORANGE), 0);
        }
    }
    
    // Bottom info
    if(fw_label){
        char txt[32]; snprintf(txt, sizeof(txt), "FW v%s", state.firmware_version);
        lv_label_set_text(fw_label, txt);
    }
    if(uptime_label){
        char txt[16]; snprintf(txt, sizeof(txt), "%02lu:%02lu:%02lu", (unsigned long)(state.uptime_sec/3600), (unsigned long)((state.uptime_sec%3600)/60), (unsigned long)(state.uptime_sec%60));
        lv_label_set_text(uptime_label, txt);
    }
    if(heap_fill && state.free_heap > 0){
        uint32_t total = 320000;
        uint32_t free_h = state.free_heap > total ? total : state.free_heap;
        int32_t fill_w = (int32_t)((free_h * 40) / total);
        if(fill_w < 2) fill_w = 2;
        if(fill_w > 40) fill_w = 40;
        lv_obj_set_width(heap_fill, fill_w);
    }
}

void DashboardScreen::play_boot_animation(){
    if(!created_) return;
    // Keep the dashboard fully opaque. Animating opacity on the 240x240 root
    // forces LVGL to allocate a full-screen alpha layer (~230 KB), which is
    // larger than the configured 64 KB LVGL heap. The failed allocation leaves
    // the draw task pending forever and stalls display refresh.
    lv_obj_set_style_opa(root, LV_OPA_COVER, 0);

    // Animate only the small arc, which renders directly into the display draw
    // buffer and does not require a full-screen intermediate layer.
    lv_anim_t arc_a;
    lv_anim_init(&arc_a);
    lv_anim_set_var(&arc_a, outer_glow_arc);
    lv_anim_set_values(&arc_a, 0, 85);
    lv_anim_set_time(&arc_a, 1200);
    lv_anim_set_exec_cb(&arc_a, anim_cb_set_arc_value);
    lv_anim_set_path_cb(&arc_a, lv_anim_path_ease_out);
    lv_anim_start(&arc_a);
}

void DashboardScreen::play_command_success(){}
void DashboardScreen::play_command_fail(){}

void DashboardScreen::destroy(){
    if(root){ lv_obj_del(root); root=nullptr; }
    created_=false;
}

} // namespace gui
} // namespace smart_device

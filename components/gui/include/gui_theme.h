#pragma once
#include "lvgl.h"

namespace smart_device {
namespace gui {
namespace theme {

// Premium Rich Modern Palette - Real Production
static const uint32_t COLOR_BG_DEEP      = 0x0A0A0F;      // deep space black
static const uint32_t COLOR_BG_CARD      = 0x1A1D26;      // card base
static const uint32_t COLOR_BG_CARD_TOP  = 0x222632;      // gradient top
static const uint32_t COLOR_BORDER       = 0x2A2F3A;
static const uint32_t COLOR_DIVIDER      = 0x1E222E;

static const uint32_t COLOR_ACCENT_CYAN  = 0x00D1FF;
static const uint32_t COLOR_ACCENT_BLUE  = 0x0A84FF;
static const uint32_t COLOR_ACCENT_GREEN = 0x00FF88;
static const uint32_t COLOR_ACCENT_ORANGE= 0xFF9F0A;
static const uint32_t COLOR_ACCENT_RED   = 0xFF3B30;

static const uint32_t COLOR_TEXT_PRIMARY = 0xFFFFFF;
static const uint32_t COLOR_TEXT_SECONDARY = 0x8A8F98;
static const uint32_t COLOR_TEXT_DIM     = 0x5A5F6A;
static const uint32_t COLOR_TEXT_ACCENT  = 0x00D1FF;

static const uint32_t COLOR_WIFI_OK      = 0x00FF88;
static const uint32_t COLOR_WIFI_BAD     = 0xFF3B30;
static const uint32_t COLOR_BLE_ADV      = 0xFF9F0A;
static const uint32_t COLOR_BLE_CONN     = 0x00FF88;

inline lv_color_t c(uint32_t hex){ return lv_color_hex(hex); }

// Premium shadow
inline void apply_premium_shadow(lv_obj_t* obj){
    lv_obj_set_style_shadow_width(obj, 20, 0);
    lv_obj_set_style_shadow_color(obj, c(0x000000), 0);
    lv_obj_set_style_shadow_opa(obj, LV_OPA_50, 0);
    lv_obj_set_style_shadow_ofs_x(obj, 0, 0);
    lv_obj_set_style_shadow_ofs_y(obj, 8, 0);
}

// Glassmorphism card
inline void apply_glass_card(lv_obj_t* obj){
    lv_obj_set_style_bg_color(obj, c(COLOR_BG_CARD), 0);
    lv_obj_set_style_bg_grad_color(obj, c(COLOR_BG_CARD_TOP), 0);
    lv_obj_set_style_bg_grad_dir(obj, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_90, 0);
    lv_obj_set_style_border_width(obj, 1, 0);
    lv_obj_set_style_border_color(obj, c(COLOR_BORDER), 0);
    lv_obj_set_style_border_opa(obj, LV_OPA_30, 0);
    lv_obj_set_style_radius(obj, 16, 0);
    apply_premium_shadow(obj);
}

} // namespace theme
} // namespace gui
} // namespace smart_device

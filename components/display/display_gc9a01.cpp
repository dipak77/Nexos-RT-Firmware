#include "display_gc9a01.h"
#include "board/board.h"
#include "device_hal/spi_hal.h"
#include "device_hal/gpio_hal.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_commands.h"
#include "esp_lcd_gc9a01.h"
#include "driver/gpio.h"
#include "mk.h"
#include <cstring>

static const char* TAG = "DISPLAY_GC9A01";

// GC9A01 power/gamma initialization is module-specific.  The generic
// esp_lcd_gc9a01 2.x defaults target a different panel revision (notably 84h,
// 89h, 8Dh, C9h, 62h/63h and 74h differ) and leave this 1.28" TFT VER1.0
// module powered and backlit but with no usable image.  This is the sequence
// used by TFT_eSPI for the exact controller/module family.
static const gc9a01_lcd_init_cmd_t kModuleInitCommands[] = {
    {0xEF, nullptr, 0, 0},
    {0xEB, "\x14", 1, 0},
    {0xFE, nullptr, 0, 0},
    {0xEF, nullptr, 0, 0},
    {0xEB, "\x14", 1, 0},
    {0x84, "\x40", 1, 0},
    {0x85, "\xFF", 1, 0},
    {0x86, "\xFF", 1, 0},
    {0x87, "\xFF", 1, 0},
    {0x88, "\x0A", 1, 0},
    {0x89, "\x21", 1, 0},
    {0x8A, "\x00", 1, 0},
    {0x8B, "\x80", 1, 0},
    {0x8C, "\x01", 1, 0},
    {0x8D, "\x01", 1, 0},
    {0x8E, "\xFF", 1, 0},
    {0x8F, "\xFF", 1, 0},
    {0xB6, "\x00\x20", 2, 0},
    {LCD_CMD_COLMOD, "\x05", 1, 0},
    {0x90, "\x08\x08\x08\x08", 4, 0},
    {0xBD, "\x06", 1, 0},
    {0xBC, "\x00", 1, 0},
    {0xFF, "\x60\x01\x04", 3, 0},
    {0xC3, "\x13", 1, 0},
    {0xC4, "\x13", 1, 0},
    {0xC9, "\x22", 1, 0},
    {0xBE, "\x11", 1, 0},
    {0xE1, "\x10\x0E", 2, 0},
    {0xDF, "\x21\x0C\x02", 3, 0},
    {0xF0, "\x45\x09\x08\x08\x26\x2A", 6, 0},
    {0xF1, "\x43\x70\x72\x36\x37\x6F", 6, 0},
    {0xF2, "\x45\x09\x08\x08\x26\x2A", 6, 0},
    {0xF3, "\x43\x70\x72\x36\x37\x6F", 6, 0},
    {0xED, "\x1B\x0B", 2, 0},
    {0xAE, "\x77", 1, 0},
    {0xCD, "\x63", 1, 0},
    {0x70, "\x07\x07\x04\x0E\x0F\x09\x07\x08\x03", 9, 0},
    {0xE8, "\x34", 1, 0},
    {0x62, "\x18\x0D\x71\xED\x70\x70\x18\x0F\x71\xEF\x70\x70", 12, 0},
    {0x63, "\x18\x11\x71\xF1\x70\x70\x18\x13\x71\xF3\x70\x70", 12, 0},
    {0x64, "\x28\x29\xF1\x01\xF1\x00\x07", 7, 0},
    {0x66, "\x3C\x00\xCD\x67\x45\x45\x10\x00\x00\x00", 10, 0},
    {0x67, "\x00\x3C\x00\x00\x00\x01\x54\x10\x32\x98", 10, 0},
    {0x74, "\x10\x85\x80\x00\x00\x4E\x00", 7, 0},
    {0x98, "\x3E\x07", 2, 0},
    {LCD_CMD_MADCTL, "\x08", 1, 0},  // BGR, rotation 0 (MX/MY applied later)
    {0x35, nullptr, 0, 0},
    {LCD_CMD_INVON, nullptr, 0, 0},
    {LCD_CMD_SLPOUT, nullptr, 0, 120},
    // DISPON is sent AFTER the first GRAM fill. Sending it here shows the
    // controller's power-on random RAM (blue scanlines) if pixel DMA fails.
};

static smart_device::Result<void> send_module_init(esp_lcd_panel_io_handle_t io) {
    for (const auto& init : kModuleInitCommands) {
        esp_err_t ret = esp_lcd_panel_io_tx_param(
            io, init.cmd, init.data, init.data_bytes);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Module init command 0x%02X failed: %s",
                     init.cmd, esp_err_to_name(ret));
            return smart_device::Result<void>::Err(
                smart_device::AppError::DISPLAY_INIT_FAILED, esp_err_to_name(ret));
        }
        if (init.delay_ms > 0) {
            mk_sleep_ms(init.delay_ms);
        }
    }
    return smart_device::Result<void>::Ok();
}

namespace smart_device {
namespace display {

static uint16_t rgb565_byteswap(uint16_t color) {
    return static_cast<uint16_t>((color << 8) | (color >> 8));
}

static Result<void> fill_color(esp_lcd_panel_handle_t panel, esp_lcd_panel_io_handle_t io,
                               int hres, int vres, uint16_t rgb565) {
    if (!panel || !io) return Result<void>::Err(AppError::DISPLAY_NOT_INITIALIZED, "no panel");
    const int lines = 16;
    const size_t row_bytes = static_cast<size_t>(hres) * sizeof(uint16_t);
    uint16_t* buf = static_cast<uint16_t*>(heap_caps_malloc(row_bytes * lines, MALLOC_CAP_DMA));
    if (!buf) buf = static_cast<uint16_t*>(malloc(row_bytes * lines));
    if (!buf) return Result<void>::Err(AppError::SYSTEM_NO_MEMORY, "fill buf");

    uint16_t wire = rgb565_byteswap(rgb565);
    for (int i = 0; i < hres * lines; ++i) buf[i] = wire;

    for (int y = 0; y < vres; y += lines) {
        int y2 = y + lines;
        if (y2 > vres) y2 = vres;
        esp_err_t ret = esp_lcd_panel_draw_bitmap(panel, 0, y, hres, y2, buf);
        if (ret != ESP_OK) {
            free(buf);
            return Result<void>::Err(AppError::DISPLAY_INIT_FAILED, esp_err_to_name(ret));
        }
    }

    // draw_bitmap/tx_color is asynchronous. A following parameter command is
    // polling and, by esp_lcd contract, first drains every queued color DMA
    // transaction. Do not free the shared color buffer before that drain.
    esp_err_t drain_ret = esp_lcd_panel_io_tx_param(io, LCD_CMD_NOP, nullptr, 0);
    if (drain_ret != ESP_OK) {
        free(buf);
        return Result<void>::Err(AppError::DISPLAY_INIT_FAILED, esp_err_to_name(drain_ret));
    }
    free(buf);
    return Result<void>::Ok();
}

GC9A01Display& GC9A01Display::instance(){ static GC9A01Display inst; return inst; }

Result<void> GC9A01Display::init(){
    if(initialized_) return Result<void>::Ok();
    auto& board = board::Board::instance();
    if(!board.is_initialized()){
        return Result<void>::Err(AppError::SYSTEM_NOT_INITIALIZED, "Board not init");
    }
    config_ = board.config();

    auto& spi = hal::SpiHal::instance();
    if(!spi.is_initialized()){
        auto res = spi.initialize(config_);
        if(res.is_err()) return res;
    }

    if (config_.lcd_bl >= 0) {
        gpio_config_t bl_cfg{};
        bl_cfg.pin_bit_mask = 1ULL << config_.lcd_bl;
        bl_cfg.mode = GPIO_MODE_OUTPUT;
        gpio_config(&bl_cfg);
        gpio_set_level(static_cast<gpio_num_t>(config_.lcd_bl), 1);
        hal::GpioHal::set_pwm(config_.lcd_bl, 100);
    }

    ESP_LOGI(TAG, "Initializing GC9A01 240x240 MOSI=%d CLK=%d CS=%d DC=%d RST=%d",
             config_.lcd_mosi, config_.lcd_clk, config_.lcd_cs, config_.lcd_dc, config_.lcd_reset);

    // R8 on the module pulls CS low if the jumper is missing, but this GC9A01
    // clone still needs a CS edge between RAMWR and the pixel payload. Let
    // esp_lcd own GPIO9 and toggle it. Do not hold CS permanently low.
    esp_lcd_panel_io_spi_config_t io_config{};
    io_config.dc_gpio_num = static_cast<gpio_num_t>(config_.lcd_dc);
    io_config.cs_gpio_num = static_cast<gpio_num_t>(config_.lcd_cs);
    io_config.pclk_hz = config_.lcd_pixel_clock_hz;
    io_config.lcd_cmd_bits = LCD_CMD_BITS;
    io_config.lcd_param_bits = LCD_PARAM_BITS;
    io_config.spi_mode = 0;
    io_config.trans_queue_depth = 10;

    esp_err_t ret = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)spi.host(), &io_config, &io_handle_);
    if(ret!=ESP_OK){
        ESP_LOGE(TAG, "Panel IO create failed: %s", esp_err_to_name(ret));
        return Result<void>::Err(AppError::DISPLAY_INIT_FAILED, esp_err_to_name(ret));
    }

    // Match the known TFT_eSPI rotation-0 setup for this module.  Element order
    // affects red/blue only, but keeping the full sequence identical removes a
    // variable during hardware bring-up.
    esp_lcd_panel_dev_config_t panel_config{};
    panel_config.reset_gpio_num = (config_.lcd_reset >= 0)
        ? static_cast<gpio_num_t>(config_.lcd_reset)
        : GPIO_NUM_NC;
    panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR;
    panel_config.bits_per_pixel = 16;

    ret = esp_lcd_new_panel_gc9a01(io_handle_, &panel_config, &panel_handle_);
    if(ret!=ESP_OK){
        ESP_LOGE(TAG, "GC9A01 panel create failed: %s", esp_err_to_name(ret));
        return Result<void>::Err(AppError::DISPLAY_INIT_FAILED, esp_err_to_name(ret));
    }

    if (config_.lcd_reset >= 0) {
        ESP_ERROR_CHECK_WITHOUT_ABORT(
            gpio_set_level(static_cast<gpio_num_t>(config_.lcd_reset), 1));
        mk_sleep_ms(50);
        ESP_ERROR_CHECK_WITHOUT_ABORT(
            gpio_set_level(static_cast<gpio_num_t>(config_.lcd_reset), 0));
        mk_sleep_ms(120);
        ESP_ERROR_CHECK_WITHOUT_ABORT(
            gpio_set_level(static_cast<gpio_num_t>(config_.lcd_reset), 1));
        mk_sleep_ms(150);
    } else {
        mk_sleep_ms(50);
    }

    // Do not call esp_lcd_panel_init() here: it sends SLPOUT before its vendor
    // table.  This module requires power/gamma setup first and SLPOUT last.
    auto init_result = send_module_init(io_handle_);
    if (init_result.is_err()) return init_result;

    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_lcd_panel_invert_color(panel_handle_, true));
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_lcd_panel_mirror(panel_handle_, false, false));
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_lcd_panel_swap_xy(panel_handle_, false));

    // Write GRAM while the panel is still blanked, then turn the scan on.
    auto fill = fill_color(panel_handle_, io_handle_, config_.lcd_hres, config_.lcd_vres, 0xF800); // red splash
    if (fill.is_err()) ESP_LOGW(TAG, "Bring-up fill failed: %s", fill.error_message().c_str());
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_lcd_panel_disp_on_off(panel_handle_, true));
    mk_sleep_ms(80);
    fill_color(panel_handle_, io_handle_, config_.lcd_hres, config_.lcd_vres, 0x0000);

    initialized_ = true;
    ESP_LOGI(TAG, "GC9A01 initialized OK %dx%d module_init=exact_sleep_order spi=%d cs=gpio%d",
             config_.lcd_hres, config_.lcd_vres, config_.lcd_pixel_clock_hz, config_.lcd_cs);
    return Result<void>::Ok();
}

Result<void> GC9A01Display::clear(uint16_t color){
    if(!panel_handle_) return Result<void>::Err(AppError::DISPLAY_NOT_INITIALIZED, "no panel");
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_lcd_panel_disp_on_off(panel_handle_, true));
    return fill_color(panel_handle_, io_handle_, config_.lcd_hres, config_.lcd_vres, color);
}

Result<void> GC9A01Display::set_brightness(uint8_t percent){
    if(config_.lcd_bl>=0){
        hal::GpioHal::set_pwm(config_.lcd_bl, percent);
    }
    ESP_LOGI(TAG, "Brightness %d%%", percent);
    return Result<void>::Ok();
}

Result<void> GC9A01Display::test_pattern(){
    if(!panel_handle_) return Result<void>::Err(AppError::DISPLAY_NOT_INITIALIZED, "no panel");
    ESP_LOGI(TAG, "Test pattern: red/green/blue/black");
    auto result = fill_color(panel_handle_, io_handle_, config_.lcd_hres, config_.lcd_vres, 0xF800);
    if (result.is_err()) return result;
    mk_sleep_ms(200);
    result = fill_color(panel_handle_, io_handle_, config_.lcd_hres, config_.lcd_vres, 0x07E0);
    if (result.is_err()) return result;
    mk_sleep_ms(200);
    result = fill_color(panel_handle_, io_handle_, config_.lcd_hres, config_.lcd_vres, 0x001F);
    if (result.is_err()) return result;
    mk_sleep_ms(200);
    return fill_color(panel_handle_, io_handle_, config_.lcd_hres, config_.lcd_vres, 0x0000);
}

} // namespace display
} // namespace smart_device

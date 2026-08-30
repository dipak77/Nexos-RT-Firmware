#include "mk.h"

#include "esp_log.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"

#include "board/board.h"
#include "device_hal/spi_hal.h"
#include "device_hal/i2c_hal.h"
#include "device_hal/usb_hal.h"
#include "display/display_gc9a01.h"
#include "lvgl_adapter/lvgl_adapter.h"
#include "gui/gui.h"
#include "gui/gui_dashboard.h"
#include "storage/nvs_store.h"
#include "storage/settings_store.h"
#include "connectivity/wifi_service.h"
#include "connectivity/ble_service.h"
#include "time_service/time_service.h"
#include "command/command_service.h"
#include "event_bus/event_bus.h"
#include "diagnostics/diagnostics.h"
#include "ota/ota_service.h"
#include "platform/esp32s3_platform.h"
#include "common/app_version.h"
#include "common/string_utils.h"
#include "lvgl.h"

#include <cstring>
#include <cstdio>
#include <ctime> // Added for strftime and struct tm

static const char* TAG = "APP_MAIN";

using namespace smart_device;

// Task forward declarations
static void gui_thread(void* arg);
static void system_thread(void* arg);
static void wifi_autoconn_thread(void* arg);

class SystemController {
public:
    esp_err_t initialize();
    esp_err_t start();
private:
    bool initialized_{false};
};

// RAII guard with finite 200ms timeout — never blocks infinite (R4)
class LvglLockGuard {
public:
    explicit LvglLockGuard(uint32_t timeout_ms = 200) : locked_(false) {
        locked_ = lvgl_adapter::LvglRuntime::instance().lock(timeout_ms);
        if (!locked_) ESP_LOGW(TAG, "LVGL lock timeout %lums — skip frame", (unsigned long)timeout_ms);
    }
    ~LvglLockGuard() { if (locked_) lvgl_adapter::LvglRuntime::instance().unlock(); }
    bool locked() const { return locked_; }
    explicit operator bool() const { return locked_; }
    LvglLockGuard(const LvglLockGuard&) = delete;
    LvglLockGuard& operator=(const LvglLockGuard&) = delete;
private:
    bool locked_;
};

// --- Helper to cleanly deregister watchdog and delete task ---
static void destroy_task(const char* name, mk_task_handle_t handle) {
    if (handle) {
        mk_watchdog_deregister(name);
        mk_task_delete(handle);
    }
}

static mk_task_handle_t spawn_nextos(const char* name, uint8_t prio, size_t stack,
                                     int core, mk_task_entry_t fn, uint32_t wdt_timeout_ms = 4000) {
    mk_task_config_t cfg{};
    cfg.name = name;
    cfg.priority = prio;
    cfg.stack_size = stack;
    cfg.stack_base = nullptr;
    cfg.core_affinity = core;
    cfg.static_alloc = false;
    
    mk_task_handle_t h = mk_task_create_ext(&cfg, fn, nullptr);
    if (!h) {
        ESP_LOGE(TAG, "[FAIL] %s spawn %s", MK_CONFIG_OS_NAME, name);
        return nullptr;
    }
    
    mk_watchdog_register(name, wdt_timeout_ms, nullptr);
    ESP_LOGI(TAG, "[PASS] %s task %s prio=%u core=%d stack=%u", 
             MK_CONFIG_OS_NAME, name, (unsigned)prio, core, (unsigned)stack);
    return h;
}

esp_err_t SystemController::initialize() {
    ESP_LOGI(TAG, "================================================");
    ESP_LOGI(TAG, " Smart Device Firmware");
    ESP_LOGI(TAG, "================================================");
    ESP_LOGI(TAG, " Firmware : %s", APP_VERSION_STRING);
    ESP_LOGI(TAG, " OS       : %s v%s", MK_CONFIG_OS_NAME, MK_CONFIG_VERSION_STRING);
    ESP_LOGI(TAG, " Hardware : %s", APP_HW_VERSION);
    ESP_LOGI(TAG, " Model    : %s", APP_MODEL);
    ESP_LOGI(TAG, " Display  : GC9A01 240x240 + LVGL 9.5 direct");
    ESP_LOGI(TAG, " Arch     : ESP32-S3 app on Core 1, ESP-IDF on Core 0");
    ESP_LOGI(TAG, " Build    : %s", BUILD_TIMESTAMP);
    ESP_LOGI(TAG, " Commit   : %s", GIT_COMMIT);
    ESP_LOGI(TAG, " IDF      : %s", IDF_TARGET_VERSION);
    ESP_LOGI(TAG, "================================================");

    // 0. Microkernel init — single_core=false: App Core1, Radio Core0 (COMPLETE_SYSTEM_REFERENCE 7)
    mk_config_t mk_cfg{};
    mk_cfg.tick_hz = MK_CONFIG_TICK_HZ;
    mk_cfg.use_preemption = true;
    mk_cfg.single_core = false;
    mk_cfg.version = MK_CONFIG_VERSION_STRING;
    if (mk_init(&mk_cfg) != MK_OK) {
        ESP_LOGE(TAG, "[FAIL] %s mk_init", MK_CONFIG_OS_NAME);
        return ESP_FAIL;
    }
    mk_diagnostics_init();
    ESP_LOGI(TAG, "[PASS] %s v%s initialized", MK_CONFIG_OS_NAME, mk_cfg.version);

    // 1. Platform shim
    platform::Esp32S3Platform::instance().init();
    ESP_LOGI(TAG, "[PASS] Platform shim initialized");

    // 2. Event loop & netif
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_netif_init());

    // 3. Event Bus
    if(!EventBus::instance().initialize()){
        ESP_LOGE(TAG, "[FAIL] Event bus init failed");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "[PASS] Event bus initialized");

    // 4. Storage & Settings
    auto nvs_res = storage::NvsStore::instance().initialize();
    if(nvs_res.is_err()){
        ESP_LOGW(TAG, "[WARN] NVS init failed: %s (continuing with defaults)", nvs_res.error_message().c_str());
    } else {
        ESP_LOGI(TAG, "[PASS] NVS initialized");
    }

    storage::SettingsStore::instance().load();
    ESP_LOGI(TAG, "[PASS] Settings loaded");

    // 5. Board & HAL
    auto board_res = board::Board::instance().initialize();
    if(board_res.is_err()){
        ESP_LOGE(TAG, "[FAIL] Board init failed");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "[PASS] Board initialized: %s", board::Board::instance().config().name);

    auto spi_res = hal::SpiHal::instance().initialize(board::Board::instance().config());
    if(spi_res.is_err()){
        ESP_LOGE(TAG, "[FAIL] SPI init failed: %s", spi_res.error_message().c_str());
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "[PASS] SPI bus initialized MOSI=%d CLK=%d",
             board::Board::instance().config().lcd_mosi, board::Board::instance().config().lcd_clk);

    auto i2c_res = hal::I2cHal::instance().initialize(board::Board::instance().config());
    if(i2c_res.is_err()){
        ESP_LOGW(TAG, "[WARN] I2C init: %s", i2c_res.error_message().c_str());
    } else {
        ESP_LOGI(TAG, "[PASS] I2C bus initialized SDA=%d SCL=%d",
                 board::Board::instance().config().i2c_sda, board::Board::instance().config().i2c_scl);
    }

    hal::UsbHal::instance().initialize_console();
    hal::UsbHal::instance().initialize_cdc();
    ESP_LOGI(TAG, "[PASS] USB/UART console initialized");

    // 6. Display GC9A01
    auto disp_res = display::GC9A01Display::instance().init();
    if(disp_res.is_err()){
        ESP_LOGE(TAG, "[FAIL] Display init failed: %s", disp_res.error_message().c_str());
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "[PASS] GC9A01 write path initialized 240x240");

    // 7. LVGL via direct adapter
    bool lvgl_ok = lvgl_adapter::LvglRuntime::instance().init(
        display::GC9A01Display::instance().panel_handle(),
        display::GC9A01Display::instance().io_handle(),
        board::Board::instance().config().lcd_hres,
        board::Board::instance().config().lcd_vres
    );
    if(!lvgl_ok){
        ESP_LOGE(TAG, "[FAIL] LVGL adapter init failed");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "[PASS] LVGL 9.5 direct adapter initialized");

    // 8. Time Service 
    time_service::TimeService::instance().initialize(storage::SettingsStore::instance().settings().timezone);
    ESP_LOGI(TAG, "[PASS] Time service initialized TZ=%s", storage::SettingsStore::instance().settings().timezone);

    // 9. Diagnostics
    diagnostics::Diagnostics::instance().initialize();
    ESP_LOGI(TAG, "[PASS] Diagnostics initialized");

    // 10. OTA Service
    ota::OtaService::instance().initialize();
    ESP_LOGI(TAG, "[PASS] OTA service initialized");

    initialized_ = true;
    return ESP_OK;
}

esp_err_t SystemController::start() {
    if(!initialized_) return ESP_FAIL;

    // Upgraded Command Thread: Loops and feeds watchdog to prevent starvation if run_console returns
    auto cmd_thread = [](void*){
        command::CommandService::instance().initialize();
        command::CommandService::instance().start();
        while(true) {
            mk_watchdog_feed_self();
            command::CommandService::instance().run_console();
            mk_sleep_ms(100); // Yield CPU and prevent tight loop if console exits
        }
    };

    mk_task_handle_t t_gui = spawn_nextos("GUI", MK_PRIO_GUI, MK_STACK_GUI, 1, gui_thread);
    if (!t_gui) return ESP_FAIL;
    
    mk_task_handle_t t_sys = spawn_nextos("SYSTEM", MK_PRIO_DIAGNOSTICS, 4096, 1, system_thread);
    if (!t_sys) { 
        destroy_task("GUI", t_gui); 
        return ESP_FAIL; 
    }
    
    mk_task_handle_t t_cmd = spawn_nextos("COMMAND", MK_PRIO_COMMAND, MK_STACK_COMMAND, 1, cmd_thread);
    if (!t_cmd) { 
        destroy_task("SYSTEM", t_sys); 
        destroy_task("GUI", t_gui); 
        return ESP_FAIL; 
    }

    // Wi-Fi
    if(storage::SettingsStore::instance().settings().wifi_enabled){
        connectivity::WifiService::instance().initialize();
        connectivity::WifiService::instance().start();
        spawn_nextos("WIFI_CONN", MK_PRIO_CONNECTIVITY, MK_STACK_CONNECTIVITY, 1, wifi_autoconn_thread);
    }

    // BLE
    if(storage::SettingsStore::instance().settings().ble_enabled){
        connectivity::BleService::instance().initialize();
    }

    if (mk_start() != MK_OK) {
        ESP_LOGE(TAG, "[FAIL] %s mk_start", MK_CONFIG_OS_NAME);
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "================================================");
    ESP_LOGI(TAG, " SYSTEM READY — %s v%s  tasks=%lu",
             MK_CONFIG_OS_NAME, MK_CONFIG_VERSION_STRING, (unsigned long)mk_task_count());
    ESP_LOGI(TAG, "================================================");
    return ESP_OK;
}

static void gui_thread(void* arg) {
    (void)arg;
    mk_watchdog_feed("GUI");
    auto& runtime = lvgl_adapter::LvglRuntime::instance();
    
    {
        LvglLockGuard lock; // RAII ensures unlock even if create() throws/asserts
        lv_obj_t* scr = lv_screen_active();
        lv_obj_set_style_bg_color(scr, lv_color_hex(0x0A0A0A), 0);
        lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
        gui::DashboardScreen::instance().create(scr);
        lv_refr_now(runtime.display());
    }
    ESP_LOGI(TAG, "GUI dashboard created and first frame flushed");

    uint64_t last = mk_time_ms();
    while(true) {
        uint64_t now = mk_time_ms();
        uint32_t dt = static_cast<uint32_t>(now - last);
        if(dt > 0) {
#if LV_VERSION_MAJOR < 9
            // LVGL 9 uses lv_tick_set_cb monotonic, no manual tick needed.
            runtime.tick(dt);
#endif
            last = now;
        }
        
        uint32_t delay = runtime.handle_timer(); // 200ms finite lock inside
        if(delay < 5) delay = 5;
        if(delay > 50) delay = 50;
        
        mk_watchdog_feed("GUI");
        mk_sleep_ms(delay);
    }
}

static void system_thread(void* arg) {
    gui::UiState ui{};
    copy_cstr(ui.firmware_version, APP_VERSION_STRING);
    copy_cstr(ui.kernel_version, MK_CONFIG_VERSION_STRING);
    uint64_t last_ui = 0;

    while(true) {
        mk_watchdog_feed("SYSTEM");
        mk_diagnostics_tick();
        EventBus::instance().process_events_blocking(50);
        
        uint64_t now = mk_time_ms();
        if(now - last_ui > 500) {
            last_ui = now;
            auto wifi_st = connectivity::WifiService::instance().status();
            auto ble_st = connectivity::BleService::instance().status();
            auto time_st = time_service::TimeService::instance().status();
            auto metrics = diagnostics::HealthMonitor::instance().get_metrics();

            // Initialize buffers with safe defaults to prevent garbage data on screen
            char time_buf[16] = "--:--";
            char date_buf[16] = "--/--";
            char time_full[16] = "--:--:--";
            
            bool use_24h = storage::SettingsStore::instance().settings().time_24h;
            time_service::TimeService::instance().get_formatted(time_buf, sizeof(time_buf), date_buf, sizeof(date_buf), use_24h);
            
            struct tm tm{};
            if(time_service::TimeService::instance().get_local_time(tm)) {
                strftime(time_full, sizeof(time_full), "%H:%M:%S", &tm);
            }

            ui.wifi_connected = (wifi_st.state == connectivity::WifiState::CONNECTED);
            ui.wifi_rssi = wifi_st.rssi;
            copy_cstr(ui.wifi_ssid, wifi_st.ssid);
            copy_cstr(ui.ip, wifi_st.ip);

            ui.ble_enabled = storage::SettingsStore::instance().settings().ble_enabled;
            ui.ble_connected = ble_st.connected;
            ui.ble_advertising = ble_st.advertising;

            ui.time_synced = time_st.synced;
            copy_cstr(ui.time_str, time_buf);
            copy_cstr(ui.date_str, date_buf);
            copy_cstr(ui.time_full, time_full);

            ui.uptime_sec = metrics.uptime_sec;
            ui.free_heap = metrics.free_heap;
            ui.brightness = storage::SettingsStore::instance().settings().display_brightness;
            ui.cpu_load = (uint8_t)mk_kernel_get_cpu_load();

            if(ui.wifi_connected && ui.time_synced) {
                copy_cstr(ui.system_status, "SYSTEM OK");
            } else if(ui.wifi_connected) {
                copy_cstr(ui.system_status, "SYNCING TIME");
            } else {
                copy_cstr(ui.system_status, "NO WIFI");
            }

            auto& runtime = lvgl_adapter::LvglRuntime::instance();
            {
                LvglLockGuard lock;
                gui::DashboardScreen::instance().update(ui);
            }
        }
    }
}

static void wifi_autoconn_thread(void* arg) {
    mk_sleep_ms(1000);
    auto& s = storage::SettingsStore::instance().settings();
    
    // Safe string check (fast path before strlen)
    if(s.wifi_ssid[0] != '\0' && strlen(s.wifi_ssid) > 0) {
        ESP_LOGI(TAG, "Attempting WiFi connection to %s...", s.wifi_ssid);
        auto res = connectivity::WifiService::instance().connect(s.wifi_ssid, s.wifi_password, 10000);
        if(res.is_ok()) {
            ESP_LOGI(TAG, "[PASS] WiFi connected IP=%s", connectivity::WifiService::instance().status().ip);
            time_service::TimeService::instance().start_sntp();
        } else {
            ESP_LOGW(TAG, "[WARN] WiFi connection failed: %s", res.error_message().c_str());
        }
    } else {
        ESP_LOGI(TAG, "No WiFi SSID configured, skipping auto-connect.");
    }
    
    mk_task_handle_t self = mk_task_self();
    if (self) {
        mk_watchdog_deregister("WIFI_CONN"); // CRITICAL: Deregister before deleting to prevent ghost watchdog triggers
        mk_task_delete(self);
    }
}

extern "C" void app_main(void) {
    static SystemController system;
    ESP_ERROR_CHECK(system.initialize());
    ESP_ERROR_CHECK(system.start());
    
    // Main task idle loop
    while(true) { 
        mk_sleep_ms(10000); 
    }
}
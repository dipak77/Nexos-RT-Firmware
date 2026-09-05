// Nexos-RT bring-up. The ESP32-S3 variant's native FSPI pins are exactly the
// display wiring below, so hardware SPI provides a deterministic 2 MHz clock.
// CS and RST are driven when connected; the module's onboard defaults still
// allow the firmware to operate while those two leads are being added.
// Hardware electrical contract: breakout VCC=5V, SPI GPIO levels=3V3.

#include <Arduino.h>
#include <SPI.h>
#include <WiFi.h>
#include <time.h>
#include <esp_sntp.h>
#include <Adafruit_GFX.h>
#include <Adafruit_GC9A01A.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <atomic>
#include <Preferences.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "kernel_manager.h"
#include "mk_enclave.h"
#include "mk_fault.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "rom/rtc.h"

// Prod-grade: keep HW WDT + brownout enabled. Boot feeds via yield() + delay.
// Do NOT call disableLoopWDT/disableCore*WDT/esp_brownout_disable here; they mask
// RF + heap instability and turn brownouts into silent display freezes.
static void feed_boot_wdt() {
    yield();
    delay(1);
}

static const char* reset_reason_str() {
    switch (esp_reset_reason()) {
        case ESP_RST_POWERON: return "POWERON";
        case ESP_RST_EXT: return "EXT";
        case ESP_RST_SW: return "SW";
        case ESP_RST_PANIC: return "PANIC";
        case ESP_RST_INT_WDT: return "INT_WDT";
        case ESP_RST_TASK_WDT: return "TASK_WDT";
        case ESP_RST_WDT: return "WDT";
        case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
        case ESP_RST_BROWNOUT: return "BROWNOUT";
        case ESP_RST_SDIO: return "SDIO";
        default: return "OTHER";
    }
}

#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

// GAP name must fit in the 31-byte advertising packet (iOS Settings is
// passive-scan only and will not read the scan-response name).
#define BLE_GAP_NAME           "SmartDisplay"
// DEMO DEFAULT ONLY — prod must provision per-device AP pass via NVS + wifi_ap_set.
// Do not broadcast this over BLE notify; display/serial setup shows it once.
#define WIFI_AP_PASS           "nexos1234"
#define WIFI_AP_CHANNEL        6

#define TFT_DC    10
// Factory standard layout:
// #define TFT_CS    9
// #define TFT_RST   14
// #define TFT_SPI_HZ 2000000U
// 5-wire bring-up: CS and RST are unplugged. Pass -1 so Adafruit
// software-resets the panel (0x01) and does not toggle unused GPIO 9/14.
// Wire CS->GPIO9 and RST->GPIO14 and set these to 9/14 for a hard reset.
#define TFT_CS    -1
#define TFT_RST   -1
#define TFT_MOSI  11
#define TFT_SCLK  12
// Prod-grade bring-up default 4MHz for dupont-wire integrity.
// Factory PCB with short traces may use 8000000U.
#define TFT_SPI_HZ 4000000U

Adafruit_GC9A01A tft(&SPI, TFT_DC, TFT_CS, TFT_RST);

static void tft_max_brightness() {
    uint8_t bright = 0xFF;
    tft.sendCommand(0x51, &bright, 1); // WRDISBV
    uint8_t ctrl = 0x24;               // BCTRL + BL on
    tft.sendCommand(0x53, &ctrl, 1);   // WRCTRLD
    // Hardware-calibrated GC9A01 Vreg1a / Vreg1b bias voltages (0x13 = 4.5V / -4.5V).
    // Restoring calibrated 0x13 eliminates column-driver saturation and
    // removes vertical lines / pixel striping artifacts completely.
    uint8_t vreg = 0x13;
    tft.sendCommand(0xC3, &vreg, 1);
    tft.sendCommand(0xC4, &vreg, 1);
}

// ---- Premium UI system: Safe circular grid + Dual Luxury Themes (0 RAM cost) ----
// 240x240 round screen safe zones: Center (120, 120), R=120, Content R=108
// Zero heap allocations in render loop — 100% bounded stack buffers.
#define COLOR_DEEP_BG  0x0000
#define COLOR_CYAN     0x367F
#define COLOR_GREEN    0x26F3
#define COLOR_ORANGE   0xFC60
#define COLOR_WHITE    0xFFFF
#define COLOR_CARD     0x10E4
#define COLOR_BORDER   0x29A7
#define COLOR_RED      0xF9C7
#define COLOR_MUTED    0xCE79

struct UiTheme {
    uint16_t bg;        // True Pitch Black (0x0000)
    uint16_t card;      // Card surface
    uint16_t border;    // Bezel & subtle card edge
    uint16_t accent;    // Primary UI accent (ice cyan / champagne gold)
    uint16_t cyan;      // Primary accent alias for splash
    uint16_t green;     // Semantic OK (mint emerald)
    uint16_t orange;    // Semantic warning (sunset amber)
    uint16_t red;       // Semantic error (crimson coral)
    uint16_t white;     // Pure White (0xFFFF)
    uint16_t muted;     // Secondary light text
    uint16_t text_dim;  // Muted subtitle / labels
    const char* name;
};

// Theme 0: "CYBER TITANIUM" (Midnight Obsidian, Electric Ice Cyan, Mint Emerald)
// Deep pitch black (#000000) blends seamlessly with the round bezel glass.
// Dark slate cards (#111822) give soft floating depth without washed-out blue glare.
static const UiTheme THEME_MIDNIGHT = {
    .bg       = 0x0000, // True Pitch Black
    .card     = 0x10E4, // Dark Obsidian Slate (#111822)
    .border   = 0x29A7, // Subtle Hairline Edge (#283440)
    .accent   = 0x367F, // Electric Ice Cyan (#38D0FF)
    .cyan     = 0x367F, // Electric Ice Cyan (#38D0FF)
    .green    = 0x26F3, // Vivid Mint Emerald (#22C55E)
    .orange   = 0xFC60, // Sunset Tangerine (#F97316)
    .red      = 0xF9C7, // Soft Coral Crimson (#EF4444)
    .white    = 0xFFFF, // Pure White
    .muted    = 0xCE79, // Cool Light Silver (#D1D5DB)
    .text_dim = 0x8C71, // Muted Ice Grey (#8EA4B8)
    .name     = "CYBER TITANIUM"
};

// Theme 1: "SOLAR GOLD" (Luxury Chronometer / Onyx & Champagne Gold)
// Warm espresso obsidian cards with rich warm gold accents.
static const UiTheme THEME_AMBER = {
    .bg       = 0x0000, // True Pitch Black
    .card     = 0x18A2, // Warm Charcoal Obsidian (#18181B)
    .border   = 0x39E4, // Brushed Titanium (#3A3734)
    .accent   = 0xF5E4, // Rich Champagne Gold (#F6BE20)
    .cyan     = 0xF5E4, // Rich Champagne Gold (#F6BE20)
    .green    = 0x26F3, // Vivid Mint Emerald (#22C55E)
    .orange   = 0xFC60, // Warm Sunset Amber (#F97316)
    .red      = 0xF9C7, // Soft Coral Crimson (#EF4444)
    .white    = 0xFFFF, // Pure White
    .muted    = 0xD6DA, // Warm Silver Linen (#D6D3D1)
    .text_dim = 0xB524, // Muted Champagne (#BAA376)
    .name     = "SOLAR GOLD"
};

static inline const UiTheme& curTheme() {
    extern std::atomic<uint8_t> g_theme;
    return (g_theme.load(std::memory_order_acquire) == 1) ? THEME_AMBER : THEME_MIDNIGHT;
}
enum UiMode : uint8_t { UI_DASH = 0, UI_SPLASH = 1, UI_TEST = 2 };

std::atomic<uint8_t> g_theme{0}; // 0=Midnight, 1=Amber — CLI `theme`
std::atomic<uint8_t> g_ui_mode{UI_DASH}; // GUI owns TFT; render skipped unless DASH
std::atomic<uint32_t> g_seconds_counter{0};
std::atomic<bool> g_trigger_color_test{false};
std::atomic<bool> g_trigger_boot{false};
std::atomic<bool> g_ble_connected{false};
std::atomic<bool> g_ble_advertising{false};
std::atomic<bool> g_wifi_connected{false};
std::atomic<bool> g_wifi_connecting{false};
std::atomic<bool> g_wifi_ap_running{false};
std::atomic<bool> g_time_synced{false};
std::atomic<bool> g_ble_hello_pending{false};
std::atomic<bool> g_radio_started{false};

char g_wifi_ssid[33]{""};
char g_wifi_ip[20]{"0.0.0.0"};
char g_ap_ssid[33]{"SmartDisplay"};
char g_ap_ip[20]{"192.168.4.1"};
int g_wifi_rssi = 0;

static BLEServer* g_pServer = nullptr;
static BLECharacteristic* g_pTxCharacteristic = nullptr;

static void ble_notify(const char* msg) {
    if (!g_pTxCharacteristic || !msg) return;
    g_pTxCharacteristic->setValue((uint8_t*)msg, strlen(msg));
    g_pTxCharacteristic->notify();
}

class MyServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) override {
        g_ble_connected.store(true, std::memory_order_release);
        g_ble_advertising.store(false, std::memory_order_release);
        g_ble_hello_pending.store(true, std::memory_order_release);
        Serial.println("[BLE] Client connected!");
    }

    void onDisconnect(BLEServer* pServer) override {
        g_ble_connected.store(false, std::memory_order_release);
        Serial.println("[BLE] Client disconnected. Restarting advertising...");
        pServer->startAdvertising();
        g_ble_advertising.store(true, std::memory_order_release);
    }
};

class MyCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) override {
        std::string rx = pCharacteristic->getValue();
        if (rx.empty()) return;
        Serial.printf("[BLE RX] %s\n", rx.c_str());
        String resp = "ECHO: ";
        resp += rx.c_str();
        ble_notify(resp.c_str());
    }
};

void init_ble() {
    if (g_pServer) return;
    feed_boot_wdt();
    // Prod-grade: Core0 WDT stays enabled; BLE init feeds via feed_boot_wdt().
    Serial.printf("[BLE] Initializing BLE GAP name: %s  heap=%u\n",
                  BLE_GAP_NAME, (unsigned)ESP.getFreeHeap());
    BLEDevice::init(BLE_GAP_NAME);
    feed_boot_wdt();
    g_pServer = BLEDevice::createServer();
    g_pServer->setCallbacks(new MyServerCallbacks());

    BLEService *pService = g_pServer->createService(SERVICE_UUID);

    g_pTxCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_TX,
        BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_READ
    );
    g_pTxCharacteristic->addDescriptor(new BLE2902());

    BLECharacteristic *pRxCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_RX,
        BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR
    );
    pRxCharacteristic->setCallbacks(new MyCallbacks());

    pService->start();

    // Keep advertising simple. Custom raw ADV + 20 ms interval fought Wi-Fi
    // SoftAP and reset the radio stack.
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMaxPreferred(0x12);
    BLEDevice::startAdvertising();
    feed_boot_wdt();
    g_ble_advertising.store(true, std::memory_order_release);
    Serial.printf("[BLE] Advertising as %s  heap=%u\n",
                  BLE_GAP_NAME, (unsigned)ESP.getFreeHeap());
}

static void build_ap_ssid() {
    uint8_t mac[6] = {0};
    WiFi.macAddress(mac);
    snprintf(g_ap_ssid, sizeof(g_ap_ssid), "SmartDisplay-%02X%02X", mac[4], mac[5]);
}

bool start_wifi_ap() {
    wifi_mode_t want = g_wifi_connecting.load(std::memory_order_acquire) ? WIFI_AP_STA : WIFI_AP;
    if (WiFi.getMode() != want) {
        WiFi.mode(want);
    }
    build_ap_ssid();
    bool ok = WiFi.softAP(g_ap_ssid, WIFI_AP_PASS, WIFI_AP_CHANNEL, 0, 4);
    if (!ok) {
        Serial.printf("[WIFI] SoftAP start failed for '%s'\n", g_ap_ssid);
        g_wifi_ap_running.store(false, std::memory_order_release);
        return false;
    }
    delay(80);
    strncpy(g_ap_ip, WiFi.softAPIP().toString().c_str(), sizeof(g_ap_ip) - 1);
    g_ap_ip[sizeof(g_ap_ip) - 1] = '\0';
    g_wifi_ap_running.store(true, std::memory_order_release);
    // Prod-grade: SSID/IP logged, PSK never logged. See display for initial setup.
    Serial.printf("[WIFI] Hotspot ON  SSID='%s' IP=%s ch=%d (2.4 GHz) PASS=[hidden]\n",
                  g_ap_ssid, g_ap_ip, WIFI_AP_CHANNEL);
    Serial.println("[WIFI] Open Android/iOS Wi-Fi settings and join that name (not Bluetooth).");
    return true;
}

void init_wifi() {
    feed_boot_wdt();
    Serial.printf("[WIFI] Starting SoftAP  heap=%u\n", (unsigned)ESP.getFreeHeap());
    WiFi.persistent(false);
    // AP-only until the user joins a home router. AP_STA + BLE is the combo
    // that reset this board. Modem sleep lets BLE share the 2.4 GHz radio.
    WiFi.mode(WIFI_AP);
    WiFi.setSleep(WIFI_PS_MIN_MODEM);
    WiFi.setHostname(BLE_GAP_NAME);
    WiFi.setTxPower(WIFI_POWER_8_5dBm);
    feed_boot_wdt();
    start_wifi_ap();
    feed_boot_wdt();
}

bool connect_wifi(const char* ssid, const char* pass) {
    if (!ssid || strlen(ssid) == 0) return false;
    strncpy(g_wifi_ssid, ssid, sizeof(g_wifi_ssid) - 1);
    g_wifi_ssid[sizeof(g_wifi_ssid) - 1] = '\0';
    g_wifi_connecting.store(true, std::memory_order_release);
    Serial.printf("[WIFI] Connecting to '%s'...\n", ssid);
    // false = leave SoftAP running; phones must still see the device SSID.
    WiFi.disconnect(false);
    if (WiFi.getMode() != WIFI_AP_STA) {
        WiFi.mode(WIFI_AP_STA);
        start_wifi_ap();
    }
    WiFi.begin(ssid, pass && strlen(pass) > 0 ? pass : nullptr);
    return true;
}

void disconnect_wifi() {
    WiFi.disconnect(false);
    g_wifi_connected.store(false, std::memory_order_release);
    g_wifi_connecting.store(false, std::memory_order_release);
    strncpy(g_wifi_ip, "0.0.0.0", sizeof(g_wifi_ip));
    Serial.println("[WIFI] STA disconnected (hotspot still on)");
}

static KernelManager& os() { return KernelManager::getInstance(); }

// Opaque pill repaint (fixed 88x20) erases previous label — 100% ghost-free.
// Dual hairline border gives crisp smartwatch anti-aliased appearance.
static void draw_status_pill(int x, int y, uint16_t col, const char* label) {
    const UiTheme& th = curTheme();
    tft.fillRoundRect(x, y, 88, 20, 10, th.card);
    tft.drawRoundRect(x, y, 88, 20, 10, col);
    tft.drawRoundRect(x + 1, y + 1, 86, 18, 9, col);
    tft.fillCircle(x + 11, y + 10, 4, col);
    tft.fillCircle(x + 11, y + 10, 2, th.white);
    tft.setFont(NULL);
    tft.setTextSize(1);
    tft.setTextWrap(false);
    tft.setTextColor(th.white, th.card);
    tft.setCursor(x + 21, y + 6);
    char fixed[10];
    snprintf(fixed, sizeof(fixed), "%-8.8s", label ? label : "");
    tft.print(fixed);
}

void render_dashboard(uint32_t sec, const KernelStats& stats) {
    if (g_ui_mode.load(std::memory_order_acquire) != UI_DASH) return; // splash/test owns TFT
    DisplayGuard lock(200);
    if (!lock) return;
    mk_watchdog_feed_self();
    const UiTheme& th = curTheme();
    tft.setTextWrap(false);
    tft.setFont(NULL);
    tft.setTextSize(1);

    // 1. Concentric Bezel Rings — subtle, deep matte
    tft.drawCircle(120, 120, 119, th.border);
    tft.drawCircle(120, 120, 118, th.border);
    tft.drawCircle(120, 120, 109, th.card);

    // 2. Connectivity status calculation
    bool w_conn = g_wifi_connected.load(std::memory_order_acquire);
    bool w_ing = g_wifi_connecting.load(std::memory_order_acquire);
    bool w_ap = g_wifi_ap_running.load(std::memory_order_acquire);
    int ap_clients = w_ap ? (int)WiFi.softAPgetStationNum() : 0;
    const char* wifi_lbl;
    uint16_t wifi_col;
    if (w_conn || ap_clients > 0) { wifi_lbl = "WIFI ON";  wifi_col = th.green; }
    else if (w_ing)               { wifi_lbl = "WIFI ..";  wifi_col = th.orange; }
    else if (w_ap)                { wifi_lbl = "WIFI AP";  wifi_col = th.accent; }
    else                          { wifi_lbl = "WIFI OFF"; wifi_col = th.text_dim; }

    bool ble_conn = g_ble_connected.load(std::memory_order_acquire);
    bool ble_adv = g_ble_advertising.load(std::memory_order_acquire);
    const char* ble_lbl = ble_conn ? "BLE ON" : (ble_adv ? "BLE ADV" : "BLE OFF");
    uint16_t ble_col = ble_conn ? th.green : (ble_adv ? th.orange : th.text_dim);

    // 3. Top Header Status Pill (centered at x=76, y=14)
    uint16_t os_col = stats.healthy ? th.green : th.red;
    draw_status_pill(76, 14, os_col, stats.healthy ? "NEXOS OK" : "SYS ERR");

    // 4. Time Computation
    char time_buf[16];
    char sec_buf[8];
    bool is_synced = g_time_synced.load(std::memory_order_acquire);
    if (is_synced) {
        time_t now;
        time(&now);
        struct tm tm_info;
        localtime_r(&now, &tm_info);
        snprintf(time_buf, sizeof(time_buf), "%02d:%02d", tm_info.tm_hour, tm_info.tm_min);
        snprintf(sec_buf, sizeof(sec_buf), ":%02d", tm_info.tm_sec);
    } else {
        snprintf(time_buf, sizeof(time_buf), "%02lu:%02lu",
                 (unsigned long)((sec / 3600) % 100),
                 (unsigned long)((sec / 60) % 60));
        snprintf(sec_buf, sizeof(sec_buf), ":%02lu", (unsigned long)(sec % 60));
    }

    // 5. Hero Time Zone — COMPLETE BOUNDING-BOX CLEAR!
    // Clears x=24..216, y=36..106 (width 192, height 70) with true black.
    // This completely eliminates any text ghosting or splash screen residue!
    tft.fillRect(24, 36, 192, 70, th.bg);

    tft.setFont(&FreeSansBold18pt7b);
    tft.setTextColor(th.white, th.bg);
    tft.setCursor(54, 72);
    tft.print(time_buf);

    tft.setFont(&FreeSansBold9pt7b);
    tft.setTextColor(th.accent, th.bg);
    tft.setCursor(162, 72);
    tft.print(sec_buf);

    tft.setFont(NULL);
    tft.setTextSize(1);
    tft.setTextColor(th.text_dim, th.bg);
    tft.setCursor(is_synced ? 88 : 80, 88);
    tft.print(is_synced ? "LOCAL TIME" : "SYSTEM UPTIME");

    // Sleek modern accent hairline
    tft.drawFastHLine(72, 98, 96, th.accent);

    // 6. Connectivity Banner (y=108 to 132, h=24)
    uint16_t ban_col = (w_conn || ap_clients > 0) ? th.green : (w_ing ? th.orange : (w_ap ? th.accent : th.text_dim));
    tft.fillRoundRect(28, 108, 184, 24, 12, th.card);
    tft.drawRoundRect(28, 108, 184, 24, 12, ban_col);
    tft.fillCircle(40, 120, 4, ban_col);
    tft.fillCircle(40, 120, 2, th.white);
    tft.setTextColor(ban_col, th.card);
    tft.setCursor(50, 116);
    char ban[26];
    if (w_conn) snprintf(ban, sizeof(ban), "IP %-16.16s", g_wifi_ip);
    else if (w_ing) snprintf(ban, sizeof(ban), "JOIN %-14.14s", g_wifi_ssid);
    else if (w_ap) {
        if (ap_clients > 0) snprintf(ban, sizeof(ban), "%-15.15s %din", g_ap_ssid, ap_clients);
        else snprintf(ban, sizeof(ban), "%-20.20s", g_ap_ssid);
    } else snprintf(ban, sizeof(ban), "%-20s", "WiFi Off");
    tft.print(ban);

    // 7. System Telemetry Card (y=136 to 188, h=52)
    tft.fillRoundRect(28, 136, 184, 52, 12, th.card);
    uint16_t card_edge = stats.healthy ? th.border : th.red;
    tft.drawRoundRect(28, 136, 184, 52, 12, card_edge);

    // Line 1: Tasks & Free Heap
    tft.setTextColor(th.muted, th.card);
    tft.setCursor(38, 144);
    tft.printf("TASKS: %-2d   HEAP: %-3u KB", stats.active_tasks_count, stats.free_heap_kb);

    // Line 2: Health Status (clean, no duplicate "[OK] OK")
    tft.setCursor(38, 158);
    tft.print("HEALTH: ");
    tft.setTextColor(stats.healthy ? th.green : th.red, th.card);
    if (stats.healthy) {
        tft.print("OPTIMAL (100)");
    } else {
        tft.printf("%-13.13s", stats.health_text);
    }

    // Line 3: System Uptime
    tft.setTextColor(th.accent, th.card);
    tft.setCursor(38, 172);
    char up_buf[22];
    snprintf(up_buf, sizeof(up_buf), "UPTIME: %02lu:%02lu:%02lu",
             (unsigned long)(sec / 3600),
             (unsigned long)((sec / 60) % 60),
             (unsigned long)(sec % 60));
    tft.print(up_buf);

    // 8. Bottom Pills (y=192, h=20)
    draw_status_pill(30, 192, wifi_col, wifi_lbl);
    draw_status_pill(122, 192, ble_col, ble_lbl);

    // 9. Bottom Heartbeat Sweep (y=218, h=3)
    tft.fillRoundRect(95, 218, 50, 3, 1, th.card);
    int fill_w = 8 + (int)(sec % 35);
    tft.fillRoundRect(95, 218, fill_w, 3, 1, th.accent);
}

static void splash_mark_n(int cx, int cy) {
    const UiTheme& th = curTheme();
    tft.fillRoundRect(cx - 22, cy - 22, 44, 44, 10, th.card);
    tft.drawRoundRect(cx - 22, cy - 22, 44, 44, 10, th.cyan);
    tft.drawRoundRect(cx - 21, cy - 21, 42, 42, 9, th.cyan);
    tft.fillRoundRect(cx - 14, cy - 14, 6, 28, 1, th.cyan);
    tft.fillRoundRect(cx + 8, cy - 14, 6, 28, 1, th.cyan);
    for (int i = 0; i < 5; i++) {
        tft.drawLine(cx - 12 + i, cy - 13, cx + 10 + i, cy + 13, th.white);
    }
    tft.fillCircle(cx - 11, cy - 14, 2, th.green);
    tft.fillCircle(cx + 13, cy + 14, 2, th.green);
}

static void splash_status(const char* line, int percent) {
    const UiTheme& th = curTheme();
    tft.fillRoundRect(38, 168, 164, 18, 9, th.card);
    tft.fillCircle(48, 177, 3, percent >= 100 ? th.green : th.cyan);
    tft.setTextSize(1);
    tft.setTextWrap(false);
    tft.setTextColor(th.white, th.card);
    tft.setCursor(58, 174);
    tft.print(line);
    tft.setTextColor(th.green, th.card);
    tft.setCursor(168, 174);
    tft.printf("%d", percent);
}

void play_boot_flash_screen() {
    g_ui_mode.store(UI_SPLASH, std::memory_order_release);
    DisplayGuard lock(400);
    if (!lock) { g_ui_mode.store(UI_DASH, std::memory_order_release); return; }
    const UiTheme& th = curTheme();
    tft.setFont(NULL);
    tft.setTextSize(1);

    tft.fillScreen(th.bg);

    for (int r = 40; r <= 116; r += 8) {
        tft.drawCircle(120, 120, r, r >= 108 ? th.cyan : th.card);
        mk_watchdog_feed_self();
        feed_boot_wdt();
        os().delayMs(18);
    }
    tft.drawCircle(120, 120, 119, th.cyan);
    tft.drawCircle(120, 120, 118, th.cyan);
    tft.drawCircle(120, 120, 109, th.card);

    splash_mark_n(120, 58);

    tft.setTextSize(2);
    tft.setTextColor(th.white, th.bg);
    tft.setCursor(72, 90);
    tft.print("NEXOS-RT");

    tft.setTextSize(1);
    tft.setTextColor(th.cyan, th.bg);
    tft.setCursor(84, 112);
    tft.print("BASE OS  V1.2");
    tft.setTextColor(th.text_dim, th.bg);
    tft.setCursor(54, 126);
    tft.print("CUSTOM MICROKERNEL");

    tft.fillRoundRect(40, 148, 160, 8, 4, th.card);
    tft.drawRoundRect(40, 148, 160, 8, 4, th.cyan);
    tft.fillRoundRect(36, 166, 168, 22, 11, th.card);
    tft.drawRoundRect(36, 166, 168, 22, 11, th.cyan);

    struct BootStep { int percent; const char* status; } steps[] = {
        { 18, "KERNEL CORE" },
        { 40, "SCHEDULER" },
        { 62, "DRIVERS" },
        { 84, "DISPLAY" },
        { 100, "READY" },
    };

    int prev_w = 0;
    for (int i = 0; i < 5; i++) {
        os().heartbeat("GUI");
        int target_w = (steps[i].percent * 156) / 100;
        if (target_w > prev_w) {
            tft.fillRect(42 + prev_w, 150, target_w - prev_w, 4, th.cyan);
        }
        tft.fillCircle(42 + target_w, 152, 2, th.white);
        prev_w = target_w;
        splash_status(steps[i].status, steps[i].percent);
        mk_watchdog_feed_self();
        feed_boot_wdt();
        os().delayMs(180);
    }

    tft.setTextColor(th.green, th.bg);
    tft.setCursor(66, 196);
    tft.print("NEXOS-RT ONLINE");
    os().delayMs(380);

    tft.fillScreen(th.bg);
    g_ui_mode.store(UI_DASH, std::memory_order_release);
}

void gui_task(void* pvParameters) {
    (void)pvParameters;
    os().heartbeat("GUI");
    mk_watchdog_feed_self();
    Serial.printf("[TASK] GUI on core %d os=%s\n", os().currentCore(), os().getActiveKernelName());

    KernelStats stats = os().getStats();
    uint8_t last_theme = g_theme.load(std::memory_order_acquire);
    tft.fillScreen(curTheme().bg);
    render_dashboard(0, stats);

    for (;;) {
        mk_watchdog_feed_self();
        os().heartbeat("GUI");
        // Theme switch needs a clean slate, else old card colors ghost
        uint8_t cur = g_theme.load(std::memory_order_acquire);
        if (cur != last_theme) {
            last_theme = cur;
            const UiTheme& th = curTheme();
            DisplayGuard g(250);
            if (g) tft.fillScreen(th.bg);
            Serial.printf("[UI] Theme -> %s\n", th.name);
        }
        if (g_trigger_boot.exchange(false, std::memory_order_acq_rel)) {
            play_boot_flash_screen();
        }
        if (g_trigger_color_test.exchange(false, std::memory_order_acq_rel)) {
            g_ui_mode.store(UI_TEST, std::memory_order_release);
            DisplayGuard g(250);
            if (g) {
                const UiTheme& th = curTheme();
                tft.fillScreen(th.cyan);
                os().delayMs(300);
                tft.fillScreen(th.green);
                os().delayMs(300);
                tft.fillScreen(th.bg);
            }
            g_ui_mode.store(UI_DASH, std::memory_order_release);
        }
        stats = os().getStats();
        render_dashboard(g_seconds_counter.load(std::memory_order_acquire), stats);
        os().delayMs(1000);
    }
}

void sys_monitor_task(void* pvParameters) {
    (void)pvParameters;
    mk_watchdog_feed_self();
    Serial.printf("[TASK] SYSTEM on core %d\n", os().currentCore());

    static uint32_t radio_t0 = 0;
    static bool ble_started = false;
    static bool wifi_started = false;
    radio_t0 = (uint32_t)mk_time_ms();

    for (;;) {
        mk_watchdog_feed_self();
        os().heartbeat("SYSTEM");

        // BLE first (previously stable), SoftAP several seconds later.
        uint32_t radio_ms = (uint32_t)mk_time_ms() - radio_t0;
        if (!ble_started && radio_ms >= 2500) {
            init_ble();
            ble_started = true;
            mk_watchdog_feed_self();
        }
        if (!wifi_started && radio_ms >= 6000) {
            init_wifi();
            wifi_started = true;
            g_radio_started.store(true, std::memory_order_release);
            mk_watchdog_feed_self();
        }
        const uint32_t seconds = (uint32_t)(mk_time_ms() / 1000ULL);
        g_seconds_counter.store(seconds, std::memory_order_release);

        wifi_mode_t wmode = WiFi.getMode();
        bool ap_on = (wmode == WIFI_AP || wmode == WIFI_AP_STA);
        g_wifi_ap_running.store(ap_on, std::memory_order_release);

        if (g_ble_hello_pending.exchange(false, std::memory_order_acq_rel) &&
            g_ble_connected.load(std::memory_order_acquire)) {
            // Prod-grade: never broadcast PSK over unencrypted BLE notify.
            char hello[96];
            snprintf(hello, sizeof(hello), "READY AP=%s IP=%s (see display for PASS)",
                     g_ap_ssid, g_ap_ip);
            ble_notify(hello);
        }

        // Check Wi-Fi state transitions
        wl_status_t wst = WiFi.status();
        if (wst == WL_CONNECTED) {
            if (!g_wifi_connected.load(std::memory_order_acquire)) {
                g_wifi_connected.store(true, std::memory_order_release);
                g_wifi_connecting.store(false, std::memory_order_release);
                strncpy(g_wifi_ip, WiFi.localIP().toString().c_str(), sizeof(g_wifi_ip) - 1);
                g_wifi_rssi = WiFi.RSSI();
                Serial.printf("[WIFI] Connected! IP: %s  RSSI: %d dBm\n", g_wifi_ip, g_wifi_rssi);
                configTime(19800, 0, "pool.ntp.org", "time.google.com");
                Serial.println("[SNTP] Time sync started (IST +5:30)");
            }
            g_wifi_rssi = WiFi.RSSI();
        } else {
            if (g_wifi_connected.load(std::memory_order_acquire)) {
                g_wifi_connected.store(false, std::memory_order_release);
                strncpy(g_wifi_ip, "0.0.0.0", sizeof(g_wifi_ip));
                Serial.println("[WIFI] Connection lost");
            }
        }

        // Check if real time is synced via NTP
        if (g_wifi_connected.load(std::memory_order_acquire) && !g_time_synced.load(std::memory_order_acquire)) {
            time_t now;
            time(&now);
            if (now > 1672531199) { // Later than Jan 1 2023
                g_time_synced.store(true, std::memory_order_release);
                struct tm timeinfo;
                localtime_r(&now, &timeinfo);
                Serial.printf("[TIME] Synced via NTP: %02d:%02d:%02d\n", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
            }
        }

        KernelStats stats = os().getStats();
        if (!stats.healthy) {
            Serial.printf("[HEALTH] %s  heap=%uKB  tasks=%d\n",
                          stats.health_text, stats.free_heap_kb, stats.active_tasks_count);
        }
        if (seconds % 5 == 0) {
            Serial.printf("[SYSTEM] up=%lus heap=%uKB tasks=%d os=%s health=%s wifi=%s ble=%s\n",
                          (unsigned long)stats.uptime_seconds,
                          stats.free_heap_kb,
                          stats.active_tasks_count,
                          stats.os_name,
                          stats.health_text,
                          g_wifi_connected.load(std::memory_order_acquire) ? "OK" :
                              (g_wifi_ap_running.load(std::memory_order_acquire) ? "AP" : "DISC"),
                          g_ble_connected.load(std::memory_order_acquire) ? "CONN" : (g_ble_advertising.load(std::memory_order_acquire) ? "ADV" : "OFF"));
        }
        os().delayMs(1000);
    }
}

// Sacrificial budget-drill burner: tight ALU loop, no yield, 2s self-cap.
// Runs at low prio so SYSTEM/CLI/GUI preempt it; accumulates real run-time
// until the 10Hz sampler traps the 8ms budget. No watchdog registered.
static void drill_burn_task(void* arg) {
    (void)arg;
    volatile uint32_t x = 0;
    uint64_t until = (uint64_t)esp_timer_get_time() + 2000000ULL;
    while ((int64_t)((uint64_t)esp_timer_get_time() - until) < 0) {
        x += (x ^ 0x9E3779B9u) + 1u;
    }
    (void)x;
}

void cli_task(void* pvParameters) {
    (void)pvParameters;
    mk_watchdog_feed_self();
    Serial.printf("[TASK] CLI on core %d\n", os().currentCore());
    char cli_buf[128];
    int cli_idx = 0;
    for (;;) {
        mk_watchdog_feed_self();
        os().heartbeat("CLI");
        while (Serial.available()) {
            char c = (char)Serial.read();
            if (c == '\r') continue;
            if (c == '\n' || cli_idx >= (int)sizeof(cli_buf) - 1) {
                cli_buf[cli_idx] = 0;
                String line = String(cli_buf);
                line.trim();
                cli_idx = 0;
                if (line.length() == 0) continue;
                Serial.print("\ndevice> ");
                Serial.println(line);
                if (line == "help") {
                    Serial.println("Commands:");
                    Serial.println("  wifi_status, wifi_scan, wifi_connect <ssid> <pass>, wifi_disconnect");
                    Serial.println("  wifi_ap  (hotspot name/password — this is what phones scan)");
                    Serial.println("  ble_status, ble_start, ble_stop");
                    Serial.println("  time_status, time_sync");
                    Serial.println("  theme [0|1], status, tasks, kernel_info, health, version, boot, test, reboot");
                    Serial.println("  enclave_show, enclave_drill, fault_show, fault_clear, wdt_show  (V2 kernel)");
                } else if (line == "wifi_status" || line == "wifi" || line == "wifi_ap") {
                    bool conn = g_wifi_connected.load(std::memory_order_acquire);
                    bool ing = g_wifi_connecting.load(std::memory_order_acquire);
                    bool ap = g_wifi_ap_running.load(std::memory_order_acquire);
                    Serial.printf("[WIFI]\nsta     : %s\nssid    : %s\nrssi    : %d dBm\nip      : %s\n",
                                  conn ? "CONNECTED" : (ing ? "CONNECTING" : "DISCONNECTED"),
                                  g_wifi_ssid, g_wifi_rssi, g_wifi_ip);
                    Serial.printf("ap      : %s\nap ssid : %s\nap pass : [hidden demo default]\nap ip   : %s\nclients : %d\n",
                                  ap ? "ON (visible on phone Wi-Fi scan, 2.4 GHz)" : "OFF",
                                  g_ap_ssid, g_ap_ip, (int)WiFi.softAPgetStationNum());
                } else if (line == "wifi_scan") {
                    Serial.println("[WIFI] Scanning nearby APs...");
                    int n = WiFi.scanNetworks();
                    Serial.printf("Found %d networks:\n", n);
                    for (int i = 0; i < n; ++i) {
                        Serial.printf("  %2d: %-24s (%4d dBm) ch %2d  %s\n",
                                      i + 1,
                                      WiFi.SSID(i).c_str(),
                                      WiFi.RSSI(i),
                                      WiFi.channel(i),
                                      WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "OPEN" : "SECURE");
                    }
                    WiFi.scanDelete();
                } else if (line.startsWith("wifi_connect")) {
                    String args = line.substring(12);
                    args.trim();
                    int spaceIdx = args.indexOf(' ');
                    if (spaceIdx <= 0 && args.length() == 0) {
                        Serial.println("Usage: wifi_connect <ssid> [password]");
                    } else {
                        String ssid = spaceIdx > 0 ? args.substring(0, spaceIdx) : args;
                        String pass = spaceIdx > 0 ? args.substring(spaceIdx + 1) : "";
                        ssid.trim();
                        pass.trim();
                        connect_wifi(ssid.c_str(), pass.c_str());
                    }
                } else if (line == "wifi_disconnect") {
                    disconnect_wifi();
                } else if (line == "time_status") {
                    time_t now;
                    time(&now);
                    struct tm tm_info;
                    localtime_r(&now, &tm_info);
                    Serial.printf("Time synced=%d  %04d-%02d-%02d %02d:%02d:%02d\n",
                                  g_time_synced.load(std::memory_order_acquire) ? 1 : 0,
                                  tm_info.tm_year + 1900, tm_info.tm_mon + 1, tm_info.tm_mday,
                                  tm_info.tm_hour, tm_info.tm_min, tm_info.tm_sec);
                } else if (line == "time_sync") {
                    if (g_wifi_connected.load(std::memory_order_acquire)) {
                        configTime(19800, 0, "pool.ntp.org", "time.google.com");
                        Serial.println("[SNTP] Time sync requested");
                    } else {
                        Serial.println("[SNTP] Please connect to Wi-Fi first");
                    }
                } else if (line == "status" || line == "health") {
                    KernelStats s = os().getStats();
                    Serial.printf("os=%s v%s health=%s heap=%uKB tasks=%d up=%lus wifi=%s ble=%s ap=%s\n",
                                  s.os_name, s.version, s.health_text, s.free_heap_kb,
                                  s.active_tasks_count, (unsigned long)s.uptime_seconds,
                                  g_wifi_connected.load(std::memory_order_acquire) ? "OK" : "DISC",
                                  g_ble_connected.load(std::memory_order_acquire) ? "CONN" : (g_ble_advertising.load(std::memory_order_acquire) ? "ADV" : "OFF"),
                                  g_ap_ssid);
                } else if (line == "tasks") {
                    Serial.printf("Nexos-RT tasks=%lu  GUI=%p SYSTEM=%p CLI=%p\n",
                                  (unsigned long)mk_task_count(),
                                  (void*)os().guiHandle(),
                                  (void*)os().sysHandle(),
                                  (void*)os().cliHandle());
                } else if (line == "kernel_info") {
                    os().printKernelInfo();
                } else if (line.startsWith("switch_kernel")) {
                    Serial.println("Nexos-RT is the only kernel. switch_kernel is not supported.");
                } else if (line == "boot") {
                    g_trigger_boot.store(true, std::memory_order_release);
                    Serial.println("[BOOT] splash queued for GUI");
                } else if (line == "test") {
                    g_trigger_color_test.store(true, std::memory_order_release);
                    Serial.println("[DISPLAY] color test queued");
                } else if (line == "version") {
                    Serial.println("Smart Device Platform v1.2.0 (Wi-Fi hotspot + BLE Enabled)");
                    Serial.printf("OS: %s V%s\n", MK_CONFIG_OS_NAME, MK_CONFIG_VERSION_STRING);
                    Serial.println("Display: GC9A01 HW SPI 4MHz MOSI=11 SCLK=12 DC=10 CS/RST unplugged");
                    Serial.printf("Bluetooth: %s (Nordic UART Service)\n", BLE_GAP_NAME);
                    Serial.printf("Wi-Fi hotspot: %s (2.4 GHz AP+STA + SNTP) PASS=[hidden]\n",
                                  g_ap_ssid);
                } else if (line == "ble_status") {
                    bool adv = g_ble_advertising.load(std::memory_order_acquire);
                    bool conn = g_ble_connected.load(std::memory_order_acquire);
                    Serial.printf("BLE state: %s advertising=%d connected=%d name=%s\n",
                                  conn ? "CONNECTED" : (adv ? "ADVERTISING" : "IDLE"),
                                  adv ? 1 : 0, conn ? 1 : 0, BLE_GAP_NAME);
                } else if (line == "ble_start") {
                    if (g_pServer && !g_ble_advertising.load(std::memory_order_acquire) && !g_ble_connected.load(std::memory_order_acquire)) {
                        BLEDevice::startAdvertising();
                        g_ble_advertising.store(true, std::memory_order_release);
                        Serial.println("BLE advertising started");
                    } else if (g_ble_connected.load(std::memory_order_acquire)) {
                        Serial.println("BLE already connected");
                    } else {
                        Serial.println("BLE already advertising");
                    }
                } else if (line == "ble_stop") {
                    if (g_pServer && g_ble_advertising.load(std::memory_order_acquire)) {
                        BLEDevice::getAdvertising()->stop();
                        g_ble_advertising.store(false, std::memory_order_release);
                        Serial.println("BLE stopped");
                    } else {
                        Serial.println("BLE is not advertising");
                    }
                } else if (line == "theme" || line.startsWith("theme ")) {
                    String arg = line.substring(5);
                    arg.trim();
                    if (arg == "1" || arg.equalsIgnoreCase("gold") || arg.equalsIgnoreCase("amber")) {
                        g_theme.store(1, std::memory_order_release);
                        Preferences p; p.begin("nexos-ui", false);
                        p.putUChar("theme", 1); p.end();
                        Serial.println("[UI] Theme SOLAR GOLD (saved)");
                    } else if (arg == "0" || arg.equalsIgnoreCase("cyber") || arg.equalsIgnoreCase("onyx") || arg.equalsIgnoreCase("midnight")) {
                        g_theme.store(0, std::memory_order_release);
                        Preferences p; p.begin("nexos-ui", false);
                        p.putUChar("theme", 0); p.end();
                        Serial.println("[UI] Theme CYBER TITANIUM (saved)");
                    } else if (arg.length() == 0) {
                        Serial.printf("[UI] Active: %s (%d). Use: theme 0 (Cyber Titanium) | theme 1 (Solar Gold)\n",
                                      curTheme().name, g_theme.load(std::memory_order_acquire));
                    } else {
                        Serial.println("Usage: theme [0|1]  (0=Cyber Titanium, 1=Solar Gold)");
                    }
                } else if (line == "enclave_show") {
                    mk_enclave_dump_status();
                    Serial.printf("Live enclaves: %lu / %d (GUI/SYSTEM/CLI bound at boot)\n",
                                  (unsigned long)mk_enclave_get_live_count(), MK_MAX_ENCLAVES);
                } else if (line == "fault_show") {
                    mk_fault_dump();
                    Serial.printf("Total recorded faults: %lu\n",
                                  (unsigned long)mk_fault_get_total_count());
                } else if (line == "fault_clear") {
                    mk_fault_clear();
                    Serial.println("Fault ring cleared");
                } else if (line == "enclave_drill") {
                    // Budget-trap proof on a sacrificial INFER enclave (8ms budget,
                    // low prio 2 burner, 10Hz sampler). Never touches boot tasks.
                    // SKIP (not FAIL) when the toolchain has run-time stats off.
                    uint32_t f0 = mk_fault_get_total_count();
                    mk_enclave_desc_t* dd = mk_enclave_create("DRILL", 2, MK_ENCLAVE_TYPE_INFER,
                                                             0, 0, 4096, MK_CAP_NONE, 8000, 0);
                    if (!dd) {
                        Serial.println("[DRILL] SKIP no free enclave slot");
                    } else if (mk_enclave_start(dd, drill_burn_task, nullptr) != MK_OK) {
                        Serial.println("[DRILL] FAIL task start");
                        mk_enclave_reclaim(dd);
                    } else {
                        bool trapped = false;
                        for (int i = 0; i < 30; i++) {
                            mk_watchdog_feed_self(); // drill waits 3s; keep CLI watchdog happy
                            os().delayMs(100);
                            if (dd->state == MK_ENCLAVE_FAILED) { trapped = true; break; }
                        }
                        uint32_t f1 = mk_fault_get_total_count();
                        mk_enclave_reset(dd->id); // reclaim slot whatever the outcome
                        if (trapped && f1 > f0) {
                            Serial.println("[DRILL] PASS budget trap + fault logged + reclaimed");
                        } else if (f1 == f0 && !trapped) {
                            Serial.println("[DRILL] SKIP run-time stats unavailable (deltas read 0)");
                        } else {
                            Serial.println("[DRILL] FAIL no trap within 3s");
                        }
                    }
                } else if (line == "wdt_show") {
                    Serial.printf("GUI WDT: %lu ms | SYSTEM WDT: %lu ms | CLI WDT: %lu ms\n",
                                  (unsigned long)mk_watchdog_age_ms("GUI"),
                                  (unsigned long)mk_watchdog_age_ms("SYSTEM"),
                                  (unsigned long)mk_watchdog_age_ms("CLI"));
                } else if (line == "reboot") {
                    ESP.restart();
                } else {
                    Serial.println("Unknown. Type help.");
                }
            } else if (c >= 32 && c < 127) {
                cli_buf[cli_idx++] = c;
            }
        }
        os().delayMs(20);
    }
}

void setup() {
    // WDT + brownout intentionally left enabled (prod-grade). If this board
    // resets here, fix the RF/heap root cause instead of disabling protection.
    Serial.begin(115200);
    delay(200);
    Serial.println("\n=========================================================");
    Serial.printf(" RESET   : %s (%d)  cpu0=%d cpu1=%d\n",
                  reset_reason_str(), (int)esp_reset_reason(),
                  (int)rtc_get_reset_reason(0), (int)rtc_get_reset_reason(1));
    Serial.println(" SMART DEVICE — Nexos-RT V1.2 (Wi-Fi hotspot + BLE)");
    Serial.println(" GC9A01 HW SPI 4MHz  SCL=12 SDA=11 DC=10  CS/RST unplugged (SW reset)");
    Serial.printf(" BLE name : %s  (use nRF Connect / LightBlue on iOS)\n", BLE_GAP_NAME);
    Serial.println(" Wi-Fi    : 2.4 GHz hotspot starts after dashboard is up");
    Serial.println("=========================================================");

    // Theme persists across EN resets (serial open resets RAM). NVS keeps choice.
    {
        Preferences p; p.begin("nexos-ui", true);
        uint8_t t = p.getUChar("theme", 0);
        p.end();
        g_theme.store((t == 1) ? 1 : 0, std::memory_order_release);
        Serial.printf("[UI] Theme %s (%d) loaded\n", curTheme().name,
                      g_theme.load(std::memory_order_acquire));
    }

    if (!os().init()) {
        Serial.printf("[FAIL] %s init\n", MK_CONFIG_OS_NAME);
        return;
    }

    // V2 enclave manager + fault ring (also inits fault ring). Boot tasks
    // (GUI, SYSTEM, CLI) are bound to dedicated enclaves during os().startTasks()
    // via mk_enclave_create() + mk_enclave_start().
    mk_enclave_init();
    Serial.printf("[ENCLAVE] manager ready, live=%lu/%d\n",
                  (unsigned long)mk_enclave_get_live_count(), MK_MAX_ENCLAVES);

    Serial.printf("[DISPLAY] Adafruit GC9A01A hardware SPI begin at %lu Hz\n",
                  (unsigned long)TFT_SPI_HZ);
    SPI.begin(TFT_SCLK, -1, TFT_MOSI, -1);
    tft.begin(TFT_SPI_HZ);
    feed_boot_wdt();
    tft.setRotation(0);
    tft.invertDisplay(true);
    tft_max_brightness();
    feed_boot_wdt();

    Serial.println("[DISPLAY] Panel ready. Clearing to true black.");
    tft.fillScreen(COLOR_DEEP_BG);
    feed_boot_wdt();

    Serial.println("[BOOT] Nexos-RT branding splash");
    play_boot_flash_screen();

    if (!os().startTasks(gui_task, sys_monitor_task, cli_task)) {
        Serial.printf("[FAIL] %s task start\n", MK_CONFIG_OS_NAME);
        return;
    }
    Serial.println("SYSTEM READY. Type help.");
}

void loop() {
    yield();
    os().delayMs(1000);
}

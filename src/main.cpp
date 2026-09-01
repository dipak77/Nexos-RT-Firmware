// Nexos-RT bring-up. The ESP32-S3 variant's native FSPI pins are exactly the
// display wiring below, so hardware SPI provides a deterministic 2 MHz clock.
// CS and RST are driven when connected; the module's onboard defaults still
// allow the firmware to operate while those two leads are being added.

#include <Arduino.h>
#include <SPI.h>
#include <WiFi.h>
#include <time.h>
#include <esp_sntp.h>
#include <Adafruit_GFX.h>
#include <Adafruit_GC9A01A.h>
#include <atomic>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "kernel_manager.h"

#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

#define TFT_DC    10
#define TFT_CS    9
#define TFT_RST   14
#define TFT_MOSI  11
#define TFT_SCLK  12
#define TFT_SPI_HZ 2000000U

Adafruit_GC9A01A tft(&SPI, TFT_DC, TFT_CS, TFT_RST);

#define COLOR_DEEP_BG  0x0000
#define COLOR_CYAN     0x07FF
#define COLOR_GREEN    0x07E0
#define COLOR_ORANGE   0xFD20
#define COLOR_WHITE    0xFFFF
#define COLOR_CARD     0x18E4
#define COLOR_BORDER   0x2966
#define COLOR_RED      0xF800

std::atomic<uint32_t> g_seconds_counter{0};
std::atomic<bool> g_trigger_color_test{false};
std::atomic<bool> g_trigger_boot{false};
std::atomic<bool> g_ble_connected{false};
std::atomic<bool> g_ble_advertising{false};
std::atomic<bool> g_wifi_connected{false};
std::atomic<bool> g_wifi_connecting{false};
std::atomic<bool> g_time_synced{false};

char g_wifi_ssid[33]{""};
char g_wifi_ip[20]{"0.0.0.0"};
int g_wifi_rssi = 0;

static BLEServer* g_pServer = nullptr;
static BLECharacteristic* g_pTxCharacteristic = nullptr;

class MyServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) override {
        g_ble_connected.store(true, std::memory_order_release);
        g_ble_advertising.store(false, std::memory_order_release);
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
        String rxValue = pCharacteristic->getValue().c_str();
        if (rxValue.length() > 0) {
            Serial.printf("[BLE RX] %s\n", rxValue.c_str());
            if (g_pTxCharacteristic) {
                String resp = "ECHO: " + rxValue;
                g_pTxCharacteristic->setValue((uint8_t*)resp.c_str(), resp.length());
                g_pTxCharacteristic->notify();
            }
        }
    }
};

void init_ble() {
    Serial.println("[BLE] Initializing BLE: SmartDisplay-BLE");
    BLEDevice::init("SmartDisplay-BLE");
    g_pServer = BLEDevice::createServer();
    g_pServer->setCallbacks(new MyServerCallbacks());

    BLEService *pService = g_pServer->createService(SERVICE_UUID);

    g_pTxCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_TX,
        BLECharacteristic::PROPERTY_NOTIFY
    );
    g_pTxCharacteristic->addDescriptor(new BLE2902());

    BLECharacteristic *pRxCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_RX,
        BLECharacteristic::PROPERTY_WRITE
    );
    pRxCharacteristic->setCallbacks(new MyCallbacks());

    pService->start();

    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();
    g_ble_advertising.store(true, std::memory_order_release);
    Serial.println("[BLE] Advertising started as SmartDisplay-BLE (ready for connection)");
}

void init_wifi() {
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    Serial.println("[WIFI] Wi-Fi STA mode initialized (ready to scan/connect)");
}

bool connect_wifi(const char* ssid, const char* pass) {
    if (!ssid || strlen(ssid) == 0) return false;
    strncpy(g_wifi_ssid, ssid, sizeof(g_wifi_ssid) - 1);
    g_wifi_connecting.store(true, std::memory_order_release);
    Serial.printf("[WIFI] Connecting to '%s'...\n", ssid);
    WiFi.disconnect(true);
    WiFi.begin(ssid, pass && strlen(pass) > 0 ? pass : nullptr);
    return true;
}

void disconnect_wifi() {
    WiFi.disconnect(true);
    g_wifi_connected.store(false, std::memory_order_release);
    g_wifi_connecting.store(false, std::memory_order_release);
    strncpy(g_wifi_ip, "0.0.0.0", sizeof(g_wifi_ip));
    Serial.println("[WIFI] Disconnected");
}

static KernelManager& os() { return KernelManager::getInstance(); }

void render_dashboard(uint32_t sec, const KernelStats& stats) {
    DisplayGuard lock(200);
    if (!lock) return;
    mk_watchdog_feed_self();

    tft.drawCircle(120, 120, 116, COLOR_CYAN);
    tft.drawCircle(120, 120, 115, COLOR_CYAN);
    tft.drawCircle(120, 120, 108, COLOR_BORDER);

    // Top-left Wi-Fi pill
    bool w_conn = g_wifi_connected.load(std::memory_order_acquire);
    bool w_ing = g_wifi_connecting.load(std::memory_order_acquire);
    uint16_t wifi_col = w_conn ? COLOR_GREEN : (w_ing ? COLOR_ORANGE : COLOR_RED);
    tft.fillRoundRect(30, 20, 58, 16, 8, COLOR_CARD);
    tft.fillCircle(38, 28, 3, wifi_col);
    tft.setTextSize(1);
    tft.setTextColor(COLOR_WHITE);
    tft.setCursor(45, 24);
    tft.print(w_conn ? "WIFI" : (w_ing ? "WIFI.." : "WIFI-"));

    // Top-right OS health pill
    tft.fillRoundRect(152, 20, 58, 16, 8, COLOR_CARD);
    tft.setCursor(158, 24);
    tft.setTextColor(COLOR_WHITE);
    tft.print(stats.healthy ? "OS:OK" : "OS:ERR");
    tft.fillCircle(198, 28, 3, stats.healthy ? COLOR_GREEN : COLOR_RED);

    // Clock or Uptime in center
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
    tft.setTextSize(3);
    tft.setTextColor(COLOR_WHITE, COLOR_DEEP_BG);
    tft.setCursor(76, 48);
    tft.print(time_buf);

    tft.setTextSize(1);
    tft.setTextColor(COLOR_CYAN, COLOR_DEEP_BG);
    tft.setCursor(168, 54);
    tft.print(sec_buf);

    tft.setCursor(is_synced ? 88 : 80, 78);
    tft.print(is_synced ? "LOCAL TIME" : "SYSTEM UPTIME");
    tft.drawFastHLine(70, 96, 100, COLOR_CYAN);

    // IP / Status banner
    tft.fillRoundRect(35, 106, 170, 20, 10, COLOR_CARD);
    tft.drawRoundRect(35, 106, 170, 20, 10, w_conn ? COLOR_GREEN : (w_ing ? COLOR_ORANGE : COLOR_BORDER));
    tft.fillCircle(45, 116, 3, w_conn ? COLOR_GREEN : (w_ing ? COLOR_ORANGE : COLOR_CYAN));
    tft.setTextColor(w_conn ? COLOR_GREEN : (w_ing ? COLOR_ORANGE : COLOR_CYAN), COLOR_CARD);
    tft.setCursor(53, 112);
    if (w_conn) {
        tft.printf("IP: %s", g_wifi_ip);
    } else if (w_ing) {
        tft.printf("JOIN: %s", g_wifi_ssid);
    } else {
        tft.print("Nexos-RT Smart Display");
    }

    tft.fillRoundRect(35, 134, 170, 48, 10, COLOR_CARD);
    uint16_t card_edge = stats.healthy ? COLOR_GREEN : COLOR_RED;
    tft.drawRoundRect(35, 134, 170, 48, 10, card_edge);
    tft.setTextColor(0xAD75, COLOR_CARD);
    tft.setCursor(45, 142);
    tft.printf("TASKS: %d", stats.active_tasks_count);
    tft.setTextColor(stats.healthy ? COLOR_GREEN : COLOR_RED, COLOR_CARD);
    tft.setCursor(45, 156);
    char health[20];
    snprintf(health, sizeof(health), "[%s] %s", stats.healthy ? "OK" : "!!",
             stats.health_text);
    tft.print(health);
    tft.setTextColor(COLOR_CYAN, COLOR_CARD);
    tft.setCursor(45, 168);
    tft.printf("HEAP: %u KB", stats.free_heap_kb);

    // Bottom status: BLE pill & Uptime
    bool is_conn = g_ble_connected.load(std::memory_order_acquire);
    bool is_adv = g_ble_advertising.load(std::memory_order_acquire);
    uint16_t ble_col = is_conn ? COLOR_GREEN : (is_adv ? COLOR_ORANGE : COLOR_RED);
    tft.fillRoundRect(36, 192, 78, 18, 9, COLOR_CARD);
    tft.drawRoundRect(36, 192, 78, 18, 9, ble_col);
    tft.fillCircle(46, 201, 3, ble_col);
    tft.setTextColor(COLOR_WHITE, COLOR_CARD);
    tft.setTextSize(1);
    tft.setCursor(54, 197);
    tft.print(is_conn ? "BLE OK" : (is_adv ? "BLE ADV" : "BLE OFF"));

    tft.setTextColor(0x7BEF, COLOR_DEEP_BG);
    tft.setCursor(140, 197);
    char up_buf[16];
    snprintf(up_buf, sizeof(up_buf), "UP %02lu:%02lu", (unsigned long)(sec / 60), (unsigned long)(sec % 60));
    tft.print(up_buf);

    tft.fillRoundRect(95, 214, 50, 3, 1, COLOR_BORDER);
    int fill_w = 12 + (int)(sec % 36);
    tft.fillRoundRect(95, 214, fill_w, 3, 1, COLOR_CYAN);
}

static void splash_mark_n(int cx, int cy) {
    tft.fillRoundRect(cx - 22, cy - 22, 44, 44, 10, COLOR_CARD);
    tft.drawRoundRect(cx - 22, cy - 22, 44, 44, 10, COLOR_CYAN);
    tft.drawRoundRect(cx - 21, cy - 21, 42, 42, 9, COLOR_BORDER);
    tft.fillRoundRect(cx - 14, cy - 14, 6, 28, 1, COLOR_CYAN);
    tft.fillRoundRect(cx + 8, cy - 14, 6, 28, 1, COLOR_CYAN);
    for (int i = 0; i < 5; i++) {
        tft.drawLine(cx - 12 + i, cy - 13, cx + 10 + i, cy + 13, COLOR_WHITE);
    }
    tft.fillCircle(cx - 11, cy - 14, 2, COLOR_GREEN);
    tft.fillCircle(cx + 13, cy + 14, 2, COLOR_GREEN);
}

static void splash_status(const char* line, int percent) {
    tft.fillRoundRect(38, 168, 164, 18, 9, COLOR_CARD);
    tft.fillCircle(48, 177, 3, percent >= 100 ? COLOR_GREEN : COLOR_CYAN);
    tft.setTextSize(1);
    tft.setTextColor(COLOR_WHITE, COLOR_CARD);
    tft.setCursor(58, 174);
    tft.print(line);
    tft.setTextColor(COLOR_GREEN, COLOR_CARD);
    tft.setCursor(168, 174);
    tft.printf("%d", percent);
}

void play_boot_flash_screen() {
    DisplayGuard lock(400);
    if (!lock) return;

    tft.fillScreen(COLOR_DEEP_BG);

    for (int r = 40; r <= 116; r += 8) {
        tft.drawCircle(120, 120, r, r >= 108 ? COLOR_CYAN : COLOR_BORDER);
        mk_watchdog_feed_self();
        os().delayMs(18);
    }
    tft.drawCircle(120, 120, 117, COLOR_CYAN);
    tft.drawCircle(120, 120, 108, COLOR_CARD);
    tft.drawFastVLine(120, 4, 10, COLOR_CYAN);
    tft.drawFastVLine(120, 226, 10, COLOR_CYAN);
    tft.drawFastHLine(4, 120, 10, COLOR_CYAN);
    tft.drawFastHLine(226, 120, 10, COLOR_CYAN);

    splash_mark_n(120, 58);

    tft.setTextSize(2);
    tft.setTextColor(COLOR_WHITE, COLOR_DEEP_BG);
    tft.setCursor(72, 90);
    tft.print("NEXOS-RT");

    tft.setTextSize(1);
    tft.setTextColor(COLOR_CYAN, COLOR_DEEP_BG);
    tft.setCursor(84, 112);
    tft.print("BASE OS  V1.2");
    tft.setTextColor(0x8410, COLOR_DEEP_BG);
    tft.setCursor(54, 126);
    tft.print("CUSTOM MICROKERNEL");

    tft.fillRoundRect(40, 148, 160, 8, 4, COLOR_CARD);
    tft.drawRoundRect(40, 148, 160, 8, 4, COLOR_BORDER);
    tft.fillRoundRect(36, 166, 168, 22, 11, COLOR_CARD);
    tft.drawRoundRect(36, 166, 168, 22, 11, COLOR_BORDER);

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
            tft.fillRect(42 + prev_w, 150, target_w - prev_w, 4, COLOR_CYAN);
        }
        tft.fillCircle(42 + target_w, 152, 2, COLOR_WHITE);
        prev_w = target_w;
        splash_status(steps[i].status, steps[i].percent);
        mk_watchdog_feed_self();
        os().delayMs(220);
    }

    tft.setTextColor(COLOR_GREEN, COLOR_DEEP_BG);
    tft.setCursor(66, 196);
    tft.print("NEXOS-RT ONLINE");
    os().delayMs(380);

    tft.fillScreen(COLOR_DEEP_BG);
}

void gui_task(void* pvParameters) {
    (void)pvParameters;
    os().heartbeat("GUI");
    mk_watchdog_feed_self();
    Serial.printf("[TASK] GUI on core %d os=%s\n", os().currentCore(), os().getActiveKernelName());

    KernelStats stats = os().getStats();
    render_dashboard(0, stats);

    for (;;) {
        mk_watchdog_feed_self();
        os().heartbeat("GUI");
        if (g_trigger_boot.exchange(false, std::memory_order_acq_rel)) {
            play_boot_flash_screen();
        }
        if (g_trigger_color_test.exchange(false, std::memory_order_acq_rel)) {
            DisplayGuard g(250);
            if (g) {
                tft.fillScreen(COLOR_CYAN);
                os().delayMs(300);
                tft.fillScreen(COLOR_GREEN);
                os().delayMs(300);
                tft.fillScreen(COLOR_DEEP_BG);
            }
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
    for (;;) {
        mk_watchdog_feed_self();
        os().heartbeat("SYSTEM");
        const uint32_t seconds = (uint32_t)(mk_time_ms() / 1000ULL);
        g_seconds_counter.store(seconds, std::memory_order_release);

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
                          g_wifi_connected.load(std::memory_order_acquire) ? "OK" : "DISC",
                          g_ble_connected.load(std::memory_order_acquire) ? "CONN" : (g_ble_advertising.load(std::memory_order_acquire) ? "ADV" : "OFF"));
        }
        os().delayMs(1000);
    }
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
                    Serial.println("  ble_status, ble_start, ble_stop");
                    Serial.println("  time_status, time_sync");
                    Serial.println("  status, tasks, kernel_info, health, version, boot, test, reboot");
                } else if (line == "wifi_status" || line == "wifi") {
                    bool conn = g_wifi_connected.load(std::memory_order_acquire);
                    bool ing = g_wifi_connecting.load(std::memory_order_acquire);
                    Serial.printf("[WIFI]\nstate : %s\nssid  : %s\nrssi  : %d dBm\nip    : %s\n",
                                  conn ? "CONNECTED" : (ing ? "CONNECTING" : "DISCONNECTED"),
                                  g_wifi_ssid, g_wifi_rssi, g_wifi_ip);
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
                    Serial.printf("os=%s v%s health=%s heap=%uKB tasks=%d up=%lus wifi=%s ble=%s\n",
                                  s.os_name, s.version, s.health_text, s.free_heap_kb,
                                  s.active_tasks_count, (unsigned long)s.uptime_seconds,
                                  g_wifi_connected.load(std::memory_order_acquire) ? "OK" : "DISC",
                                  g_ble_connected.load(std::memory_order_acquire) ? "CONN" : (g_ble_advertising.load(std::memory_order_acquire) ? "ADV" : "OFF"));
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
                    Serial.println("Smart Device Platform v1.2.0 (Wi-Fi + BLE Enabled)");
                    Serial.printf("OS: %s V%s\n", MK_CONFIG_OS_NAME, MK_CONFIG_VERSION_STRING);
                    Serial.println("Display: GC9A01 HW SPI 2MHz MOSI=11 SCLK=12 DC=10 CS=9 RST=14");
                    Serial.println("Bluetooth: SmartDisplay-BLE (Nordic UART Service)");
                    Serial.println("Wi-Fi: 802.11 b/g/n Station + SNTP Time Sync");
                } else if (line == "ble_status") {
                    bool adv = g_ble_advertising.load(std::memory_order_acquire);
                    bool conn = g_ble_connected.load(std::memory_order_acquire);
                    Serial.printf("BLE state: %s advertising=%d connected=%d name=SmartDisplay-BLE\n",
                                  conn ? "CONNECTED" : (adv ? "ADVERTISING" : "IDLE"),
                                  adv ? 1 : 0, conn ? 1 : 0);
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
    Serial.begin(115200);
    delay(400);
    Serial.println("\n=========================================================");
    Serial.println(" SMART DEVICE — Nexos-RT V1.2 (Wi-Fi + BLE Enabled)");
    Serial.println(" GC9A01 HW SPI 2MHz  SCL=12 SDA=11 DC=10 CS=9 RST=14");
    Serial.println(" BLE Device Name: SmartDisplay-BLE");
    Serial.println(" Wi-Fi Station Mode: Ready");
    Serial.println("=========================================================");

    if (!os().init()) {
        Serial.printf("[FAIL] %s init\n", MK_CONFIG_OS_NAME);
        return;
    }

    Serial.printf("[DISPLAY] Adafruit GC9A01A hardware SPI begin at %lu Hz\n",
                  (unsigned long)TFT_SPI_HZ);
    SPI.begin(TFT_SCLK, -1, TFT_MOSI, -1);
    tft.begin(TFT_SPI_HZ);
    tft.setRotation(0);
    tft.invertDisplay(true);

    Serial.println("[DISPLAY] startup test RED -> GREEN -> BLUE -> BLACK");
    tft.fillScreen(COLOR_RED);
    delay(220);
    tft.fillScreen(COLOR_GREEN);
    delay(220);
    tft.fillScreen(0x001F);
    delay(220);
    tft.fillScreen(COLOR_DEEP_BG);

    Serial.println("[BOOT] Nexos-RT branding splash");
    play_boot_flash_screen();

    // Initialize Wi-Fi and Bluetooth stacks
    init_wifi();
    init_ble();

    if (!os().startTasks(gui_task, sys_monitor_task, cli_task)) {
        Serial.printf("[FAIL] %s task start\n", MK_CONFIG_OS_NAME);
        return;
    }
    Serial.println("SYSTEM READY. Type help.");
}

void loop() {
    os().delayMs(1000);
}

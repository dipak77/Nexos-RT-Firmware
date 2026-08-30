// Nexos-RT bring-up. The ESP32-S3 variant's native FSPI pins are exactly the
// display wiring below, so hardware SPI provides a deterministic 2 MHz clock.
// CS and RST are driven when connected; the module's onboard defaults still
// allow the firmware to operate while those two leads are being added.

#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_GC9A01A.h>
#include <atomic>
#include "kernel_manager.h"

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

static KernelManager& os() { return KernelManager::getInstance(); }

void render_dashboard(uint32_t sec, const KernelStats& stats) {
    DisplayGuard lock(200);
    if (!lock) return;
    mk_watchdog_feed_self();

    tft.drawCircle(120, 120, 116, COLOR_CYAN);
    tft.drawCircle(120, 120, 115, COLOR_CYAN);
    tft.drawCircle(120, 120, 108, COLOR_BORDER);

    tft.fillRoundRect(34, 20, 52, 16, 8, COLOR_CARD);
    tft.fillCircle(42, 28, 3, COLOR_GREEN);
    tft.setTextSize(1);
    tft.setTextColor(COLOR_WHITE);
    tft.setCursor(49, 24);
    tft.print("SPI");

    tft.fillRoundRect(154, 20, 52, 16, 8, COLOR_CARD);
    tft.setCursor(160, 24);
    tft.setTextColor(COLOR_WHITE);
    tft.print("OS");
    tft.fillCircle(198, 28, 3, COLOR_GREEN);

    tft.setTextSize(3);
    tft.setTextColor(COLOR_WHITE, COLOR_DEEP_BG);
    tft.setCursor(76, 48);
    char time_buf[8];
    snprintf(time_buf, sizeof(time_buf), "%02lu:%02lu",
             (unsigned long)((sec / 3600) % 100),
             (unsigned long)((sec / 60) % 60));
    tft.print(time_buf);

    tft.setTextSize(1);
    tft.setTextColor(COLOR_CYAN, COLOR_DEEP_BG);
    tft.setCursor(168, 54);
    char sec_buf[8];
    snprintf(sec_buf, sizeof(sec_buf), ":%02lu", (unsigned long)(sec % 60));
    tft.print(sec_buf);

    tft.setCursor(80, 78);
    tft.print("SYSTEM UPTIME");
    tft.drawFastHLine(70, 96, 100, COLOR_CYAN);

    tft.fillRoundRect(50, 106, 140, 20, 10, COLOR_CARD);
    tft.drawRoundRect(50, 106, 140, 20, 10, COLOR_ORANGE);
    tft.fillCircle(62, 116, 3, COLOR_ORANGE);
    tft.setTextColor(COLOR_ORANGE, COLOR_CARD);
    tft.setCursor(70, 112);
    tft.print("Nexos-RT");

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

    tft.setTextColor(0x7BEF, COLOR_DEEP_BG);
    tft.setCursor(45, 198);
    tft.print("FW V1.2");
    tft.setCursor(150, 198);
    char up_buf[16];
    snprintf(up_buf, sizeof(up_buf), "%02lu:%02lu", (unsigned long)(sec / 60), (unsigned long)(sec % 60));
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
        KernelStats stats = os().getStats();
        if (!stats.healthy) {
            Serial.printf("[HEALTH] %s  heap=%uKB  tasks=%d\n",
                          stats.health_text, stats.free_heap_kb, stats.active_tasks_count);
        }
        if (seconds % 5 == 0) {
            Serial.printf("[SYSTEM] up=%lus heap=%uKB tasks=%d os=%s health=%s\n",
                          (unsigned long)stats.uptime_seconds,
                          stats.free_heap_kb,
                          stats.active_tasks_count,
                          stats.os_name,
                          stats.health_text);
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
                    Serial.println("help, status, tasks, kernel_info, health");
                    Serial.println("boot, test, version, reboot");
                } else if (line == "status" || line == "health") {
                    KernelStats s = os().getStats();
                    Serial.printf("os=%s v%s health=%s heap=%uKB tasks=%d up=%lus\n",
                                  s.os_name, s.version, s.health_text, s.free_heap_kb,
                                  s.active_tasks_count, (unsigned long)s.uptime_seconds);
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
                    Serial.println("Smart Device Platform v1.2.0");
                    Serial.printf("OS: %s V%s\n", MK_CONFIG_OS_NAME, MK_CONFIG_VERSION_STRING);
                    Serial.println("Display: GC9A01 HW SPI 2MHz MOSI=11 SCLK=12 DC=10 CS=9 RST=14 VCC=3V3");
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
    Serial.println(" SMART DEVICE — Nexos-RT V1.2");
    Serial.println(" GC9A01 HW SPI 2MHz  SCL=12 SDA=11 DC=10 CS=9 RST=14");
    Serial.println(" TFT VER1.0 VCC=3V3 only (never USB 5V)");
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

    // A direct full-frame test proves command, clock, data and GRAM writes
    // before the scheduler or dashboard can obscure a panel fault.
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

    if (!os().startTasks(gui_task, sys_monitor_task, cli_task)) {
        Serial.printf("[FAIL] %s task start\n", MK_CONFIG_OS_NAME);
        return;
    }
    Serial.println("SYSTEM READY. Type help.");
}

void loop() {
    os().delayMs(1000);
}

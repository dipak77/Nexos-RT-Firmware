# Nexos-RT Production Hardening Plan - Super Solid v4
**Consolidated from: My Review + DeepSeek AI + Qwen AI + ChatGPT**
**Brand: Next OS -> Nexos-RT**
**Date: 2026-08-30**
**Maturity: 5/10 Prototype -> Target 9/10 Production**

---

## 1. Executive Summary

The firmware is a dual-layer ESP32-S3 architecture: Custom "Next OS" microkernel wrapping FreeRTOS, Arduino bring-up with Adafruit GFX, and ESP-IDF production stack with LVGL, EventBus, HALs, OTA.

**Root Cause Found by ChatGPT:** Nexos-RT is NOT an independent microkernel today. It is a FreeRTOS abstraction layer where `xTaskCreatePinnedToCore()` runs tasks immediately, but `mk_start()` is assumed to start them later. This creates race between task run and watchdog registration.

**Critical Count:** 18 P0 bugs, 11 P1 gaps. 5 new bugs found by Qwen, 7 new architectural flaws found by ChatGPT that explain all others.

**Rename:** All visible strings `Next OS` -> `Nexos-RT`, `NEXT OS` -> `NEXOS-RT`. Keep `mk_` code prefix for API compat.

---

## 2. Consolidated Critical Bugs (P0) - All 4 Reviews

| ID | Description | Severity | Found By | Files | Fix Summary |
|---|---|---|---|---|---|
| C1 | SMP race `s_tasks[]`, `s_task_count`, `s_next_id` no lock | P0 | DeepSeek, Qwen, Me, ChatGPT | mk_task.c | Add `portMUX_TYPE` spinlock, atomic ops |
| C2 | `s_current_task` single global for dual-core S3 - overwritten by both cores | P0 | All 4 | mk_task.c | Delete global, use `xTaskGetCurrentTaskHandle()` |
| C3 | Task slot leak - if xTaskCreate fails, slot id!=0 remains, never freed | P0 | ChatGPT NEW | mk_task.c | Add slot state FREE/RESERVED/LIVE/DELETING, rollback on fail |
| C4 | TCB abuse `stack_base=arg`, `stack_pointer=entry` - UB + breaks debugger/stack check/MPU | P0 | Me, Qwen, ChatGPT | mk_task.c | Dedicated fields `entry`, `arg` in internal struct |
| C5 | `mk_start()` has no semantics - FreeRTOS already running, tasks run before it | P0 | ChatGPT NEW Root Cause | kernel_manager.cpp, app_main.cpp | Launch gate EventGroup, tasks wait for `mk_start()` |
| C6 | `single_core=true` vs pinning GUI to core 1, SYSTEM/CLI to core 0 contradictory | P0 | Me, ChatGPT | kernel_manager.cpp, app_main.cpp | Set `single_core=false`, pin app to Core1, system to Core0 |
| C7 | Two schedulers, only FreeRTOS real - `mk_scheduler_add_ready()` fake | P0 | ChatGPT NEW | mk_task.c, mk_scheduler.c | Choose Arch A: Nexos-RT = runtime over IDF, delete fake ready list |
| C8 | Infinite display lock `0xFFFFFFFFu` blocks + starves 4s WDT | P0 | DeepSeek, Qwen, Me, ChatGPT | arduino_main.cpp | Finite 200ms + retry + feed WDT, split splash locks |
| C9 | CLI blocking `readStringUntil('\n')` blocks 1s, delays heartbeat | P0 | Qwen NEW | arduino_main.cpp, cli_task | Non-blocking byte state machine |
| C10 | COMMAND thread `CommandService::start()` blocking before feed loop never reached | P0 | Qwen NEW | app_main.cpp | Make start() non-blocking or feed inside service |
| C11 | Time drift `g_seconds_counter++` + `delay(1000)` - RTOS jitter | P0 | Qwen NEW | arduino_main.cpp | Derive from `mk_time_ms()/1000` or SNTP |
| C12 | Startup rollback leak - if CLI fails, GUI+SYSTEM orphaned | P0 | Qwen NEW | kernel_manager.cpp | Delete already spawned tasks on failure |
| C13 | Priority mapping `8+priority` puts app above WiFi/TCP/IP, starves IDLE | P0 | Me, ChatGPT | mk_task.c, app_main.cpp | Explicit table MK_CRITICAL/REALTIME/HIGH/NORMAL/LOW |
| C14 | Fake CPU load `context_switches % 100` random | P0 | DeepSeek, Qwen, ChatGPT | app_main.cpp | Real calc: `100*(1-idle_delta/total_delta)` per core |
| C15 | GUI loses time `dt>50? dt=50; last=now` loses 450ms | P0 | ChatGPT NEW | app_main.cpp | Use monotonic tick callback `lv_tick_set_cb()` |
| C16 | Health text dangling `stats.health_text = mk_diagnostics_health_text()` | P0 | DeepSeek, Me | kernel_manager.cpp | Copy to fixed buffer |
| C17 | Watchdog name-based `feed("GUI")` - duplicate/rename race | P1->P0 | ChatGPT NEW | kernel_manager.cpp, mk_task.c | Handle-based `feed(token)` or `feed_self()` |
| C18 | NVS erase return unchecked `nvs_flash_erase()` ignored | P1 | ChatGPT NEW | kernel_manager.cpp | Check erase rc, distinguish missing/corrupt/version |

---

## 3. High Gaps (P1) - Combined

- **G1 Error handling cleanup:** Partial init leaks mutex, no RAII
- **G2 Thread safety `getStats()` const but calls `mk_diagnostics_tick()` mutating**
- **G3 Stack sizes hardcoded 8192/4096 no watermark alert**
- **G4 Watchdog magic 4000 should be per-task config**
- **G5 Excessive unconditional `Serial.printf` / `ESP_LOGI`**
- **G6 Display coords/colors hardcoded, no theme**
- **G7 LVGL locking granularity - `system_thread` locks whole `DashboardScreen::update()`**
- **G8 O(n) task search `mk_task_self()` linear scan - free list/bitmap better**
- **G9 No unit tests for kernel layer**
- **G10 NVS degraded mode - continues but storage assumes OK**
- **G11 PlatformIO `src_dir=main` + `build_src_filter=+<arduino_main.cpp>` includes cpp hack - ODR violation**
- **G12 Two competing architectures - Arduino vs IDF should split bringup vs prod**
- **G13 Hardcoded `core_count=2`, `"2 x Xtensa"` should use `esp_chip_info()`**
- **G14 Arduino SW SPI bit-bang slow, starves tasks - Qwen**
- **G15 Lambda to function ptr decay fragile `auto cmd_thread=[](void*){}` - ChatGPT**
- **G16 `kernel_version` receives OS name bug `copy_cstr(ui.kernel_version, MK_CONFIG_OS_NAME)`**
- **G17 Service status queries no lifecycle guarantee - need ServiceState enum**
- **G18 Init error philosophy inconsistent `ESP_ERROR_CHECK` fatal vs `return error`**

---

## 4. Existing Strengths to Preserve (DeepSeek + Qwen + ChatGPT agree)

- Clear separation KernelManager, Board, HAL, Storage, Connectivity, EventBus
- Display mutex present in both stacks
- Watchdog register/feed pattern
- Graceful degradation: NVS, I2C, Time, OTA warn not crash - IDF `SystemController::initialize()` masterclass
- Structured logging with TAG
- Result<T> error type
- LVGL adapter with lock + tick clamping to avoid IDLE starvation
- Build scripts `pio_kernel_sources.py`
- Comments explain SW SPI reason, LVGL clamp, Core1 usage
- Singleton pattern for managers
- Modern C++ in IDF layer: RAII, namespace, singleton

---

## 5. Target Architecture - Super Solid

### 5.1 Decision: Nexos-RT Identity
**Architecture A - Recommended:** Nexos-RT = Premium Runtime / OS Abstraction over ESP-IDF, NOT competing kernel.

```
Application: Device, Connectivity, UI, CLI, OTA
        ↓ Only #include <nexos/*.h> NEVER freertos/*
Nexos-RT: Task, Mutex, Queue, Event, Timer, Watchdog[handle], Diagnostics[real], EventBus, Lifecycle
        ↓ CHIP PORT API
ESP32-S3 Port: task_port.cpp [xTaskCreate], critical_port.cpp [portMUX], time_port.cpp [esp_timer], heap_port.cpp
        ↓
ESP-IDF: FreeRTOS + WiFi + BLE + TCP/IP [Core 0]
```

### 5.2 Core Affinity
- **Core 0 - System Domain:** WiFi, BLE, TCP/IP, NVS, OTA, USB, netif, event_loop
- **Core 1 - Application Domain:** All Nexos-RT tasks - GUI, SYSTEM, COMMAND, WIFI_CONN

All Nexos-RT tasks pinned to 1 initially. `single_core=false` but policy enforces Core1.

### 5.3 Task Registry - Fixed

```c
typedef enum { SLOT_FREE, SLOT_RESERVED, SLOT_LIVE, SLOT_DELETING } mk_slot_state_t;

typedef struct {
  mk_task_t public_tcb;
  TaskHandle_t port_handle;
  mk_task_entry_t entry; // REAL field, not abuse
  void* arg;             // REAL field
  void* stack_base_real;
  size_t stack_size_real;
  char name[32];
  mk_slot_state_t slot_state;
  mk_watchdog_token_t wdt_token;
} mk_task_internal_t;

static portMUX_TYPE s_task_lock = portMUX_INITIALIZER_UNLOCKED;
```

Every create/delete/lookup wrapped in `taskENTER_CRITICAL(&s_task_lock)`.

### 5.4 Launch Gate - Fix C5

```c
static EventGroupHandle_t s_start_gate;
#define NEXOS_START_BIT 0x01

static void mk_port_task_wrapper(void* p){
  xEventGroupWaitBits(s_start_gate, NEXOS_START_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
  tramp->entry(tramp->arg);
  mk_task_delete(NULL);
}

mk_status_t mk_init(mk_config_t* cfg){
  s_start_gate = xEventGroupCreate();
  // ...
}
mk_status_t mk_start(){
  // all watchdogs registered, all tasks created
  xEventGroupSetBits(s_start_gate, NEXOS_START_BIT);
  return MK_OK;
}
```

### 5.5 Priority Map - Fix C13

```c
typedef enum {
  NEXOS_PRIO_IDLE=0,
  NEXOS_PRIO_BACKGROUND=2,
  NEXOS_PRIO_LOW=5,      // COMMAND
  NEXOS_PRIO_NORMAL=8,   // WIFI_CONN
  NEXOS_PRIO_HIGH=12,    // SYSTEM
  NEXOS_PRIO_REALTIME=18,// GUI
  NEXOS_PRIO_CRITICAL=20
} nexos_prio_t;

// Map to FreeRTOS: no +8 hack, direct but capped below WiFi 22-23
uint32_t port_prio = nexos_prio_t; // max 20 < 23 WiFi
```

### 5.6 Watchdog Handle-Based - Fix C17

```c
mk_watchdog_token_t token;
mk_task_handle_t h = mk_task_create_ext(&cfg, entry, arg);
mk_watchdog_register(h, 4000, &token);
mk_watchdog_feed(token);
mk_watchdog_feed_self(); // auto from TCB
```

### 5.7 Lifecycle - Explicit [ChatGPT]

Kernel:
```
UNINITIALIZED -> INITIALIZING -> INITIALIZED -> STARTING -> RUNNING -> STOPPING -> STOPPED
```
Reject `mk_task_create()` before INITIALIZED, double start -> `MK_ERR_BAD_STATE`.

Task:
```
FREE -> CREATED -> READY -> RUNNING <-> BLOCKED/SUSPENDED -> TERMINATING -> ZOMBIE -> FREE
```

Service:
```cpp
enum class ServiceState { UNINITIALIZED, DISABLED, STARTING, RUNNING, DEGRADED, FAILED, STOPPING };
```

---

## 6. Premium Rich UI Architecture - Fix Display Races

### 6.1 Single Owner [ChatGPT C19]

**Before:** GUI, CLI, Boot all lock display_lock_ -> contention, infinite lock.

**After:**
```
Other Tasks -> UiModel/UiCommand Queue -> GUI Task [sole LVGL owner] -> GC9A01Display driver
```

No other task touches TFT. Fixes C8.

### 6.2 LVGL Tick - Fix C15

Don't:
```cpp
dt = now-last; if(dt>50) dt=50; tick(dt); last=now; // loses 450ms
```

Do:
```cpp
// In LvglRuntime::init()
lv_tick_set_cb([](){ return mk_time_ms(); });
// or esp_timer every 1ms calls lv_tick_inc(1)
```

### 6.3 Time - Fix C11

```cpp
// system_thread
g_seconds_counter = mk_time_ms()/1000ULL; // monotonic, no drift
// Or use TimeService SNTP RTC
```

### 6.4 Theme - Fix G6

```cpp
struct NexosTheme {
  uint16_t bg = 0x0000;
  uint16_t card = 0x18E4;
  uint16_t border = 0x2966;
  uint16_t cyan = 0x07FF;
  uint16_t green = 0x07E0;
  uint16_t orange = 0xFD20;
};
```

No hardcoded coords, adapt to 240x240.

### 6.5 Boot Flow - Premium Stages

```
00 Reset reason + crash record
01 Memory heap caps
02 Nexos-RT core + start_gate create
03 Board ID via esp_chip_info(), flash, PSRAM
04 NVS robust [distinguish missing/corrupt/version]
05 HAL SPI/I2C/USB
06 Display HW init
07 LVGL + tick source
08 EventBus
09 Diagnostics + WDT
10 GUI task create [waiting on gate]
11 Network optional
12 Time SNTP optional
13 COMMAND + CLI
14 mk_start() -> open gate
15 RUNNING
```

Each component declares REQUIRED / OPTIONAL / DEGRADED_ALLOWED.

---

## 7. Build & Repo Layout [ChatGPT]

```
firmware/
 apps/
  production/app_main.cpp      // IDF prod
  bringup/arduino_main.cpp      // Arduino display validation only
 components/
  nexos/
   include/nexos/task.h, mutex.h, queue.h, watchdog.h, diagnostics.h
   kernel/, ipc/, memory/, time/, diagnostics/
  nexos_port_esp32s3/
   task_port.cpp, critical_port.cpp, time_port.cpp, heap_port.cpp, cpu_port.cpp
  board/, hal/spi,i2c,usb,gpio/, display/, gui/, connectivity/, storage/, time_service/, ota/, diagnostics/, command/
 boards/esp32s3_gc9a01/board_config.hpp, sdkconfig.defaults
 test/unit/, integration/, stress/, hil/
 tools/
```

PlatformIO:
```ini
[env:esp32s3_arduino]
framework=arduino
src_dir=apps/bringup

[env:esp32s3_idf]
framework=espidf
build_flags=-I components/nexos/include
```

Delete `build_src_filter = +<arduino_main.cpp>` hack.

---

## 8. Nexos-RT Rename Plan

**Branding:**
Human: Nexos-RT, Upper: NEXOS-RT, Log: [NEXOS]

Keep `mk_` prefix internally for compat, only string changes.

**Files to touch:**
- `mk_config.h`: `#define MK_CONFIG_OS_NAME "Nexos-RT"`
- `kernel_manager.cpp`: `[KERNEL] Nexos-RT ready.`, `[PASS] Nexos-RT task`
- `arduino_main.cpp`: `tft.print("Nexos-RT")`, `tft.print("NEXOS-RT")`, `NEXOS-RT ONLINE`, `================ NEXOS-RT ================`
- `app_main.cpp`: logs auto via config
- `cli_task`: `Nexos-RT is the active kernel`

**Script:**
```bash
find . -type f \( -name "*.cpp" -o -name "*.h" -o -name "*.c" \) -exec sed -i 's/Next OS/Nexos-RT/g; s/NEXT OS/NEXOS-RT/g' {} \;
grep -R "Next OS" --include="*.cpp" --include="*.h" .
```

**Splash Update:**
```cpp
// N + RT subscript premium
tft.setTextSize(2); tft.print("N");
tft.setTextSize(1); tft.print("RT");
```

---

## 9. Implementation Roadmap - Ordered [ChatGPT order merged]

### Phase 0: Hotfix 3h - Blocks boot
- [ ] **C10** Fix COMMAND WDT starvation - make start() non-blocking or feed inside
- [ ] **C9** Replace `readStringUntil` with non-blocking byte machine + feed WDT
- [ ] **C11** `g_seconds_counter = mk_time_ms()/1000`
- [ ] **C12** Rollback on spawn fail - delete already created tasks
- [ ] **C5** Add start_gate EventGroup
Tasks: kernel_manager.cpp, app_main.cpp, arduino_main.cpp

### Phase 1: Kernel Correctness 1d
- [ ] **C1,C2,C3,C4** Rewrite mk_task.c with slot states, spinlock, real entry/arg fields, no s_current_task
- [ ] **C6** single_core=false, pin app to Core1
- [ ] **C7** Delete fake scheduler or sync with FreeRTOS state
- [ ] **C16** Copy health_text to buffer
- [ ] **C18** Check nvs_flash_erase() rc
Files: mk_task.c, mk_config.h, kernel_manager.cpp

### Phase 2: Concurrency & IO 1.5d
- [ ] **C8** Split splash locks per chunk, finite timeout + retry
- [ ] **C17** Handle-based WDT
- [ ] **C13** Priority table redesign
- [ ] **C15** LVGL tick callback
- [ ] **C14** Real CPU load per core
- [ ] G1,G2,G3,G4 RAII DisplayGuard, log_lock, watermark alerts
Files: all

### Phase 3: UI & Build 2d
- [ ] Single GUI owner, UiCommand queue
- [ ] Theme struct, no hardcoded coords
- [ ] HW SPI for Arduino bringup, not SW bit-bang
- [ ] PlatformIO env fix, repo layout split bringup/prod
- [ ] G11,G12,G13,G14,G15 fixes

### Phase 4: Prod Hardening 2d
- [ ] PSRAM LVGL buffers `heap_caps_malloc(MALLOC_CAP_SPIRAM)`
- [ ] ServiceState state machines
- [ ] Boot stages with REQUIRED/OPTIONAL
- [ ] Unit tests: task create/delete race, display lock timeout, WDT
- [ ] Rename Next OS -> Nexos-RT across codebase
- [ ] Diagnostics: kernel_info, tasks table with real numbers

---

## 10. Patch Examples

### Non-blocking CLI [Qwen fix]
```cpp
char cli_buf[128]; int idx=0;
for(;;){
  mk_watchdog_feed_self();
  while(Serial.available()){
    char c=Serial.read();
    if(c=='\n' || idx>=127){ cli_buf[idx]='\0'; handle(cli_buf); idx=0; }
    else if(c!='\r'){ cli_buf[idx++]=c; }
  }
  mk_sleep_ms(20);
}
```

### DisplayGuard RAII [Me]
```cpp
class DisplayGuard {
  bool locked;
public:
  DisplayGuard(uint32_t ms): locked(KernelManager::getInstance().lockDisplay(ms)) {}
  ~DisplayGuard(){ if(locked) KernelManager::getInstance().unlockDisplay(); }
  explicit operator bool() const { return locked; }
};
```

### Rollback [Qwen]
```cpp
gui_ = spawn(...); if(!gui_) return false;
sys_ = spawn(...); if(!sys_){ mk_task_delete(gui_); gui_=nullptr; return false; }
cli_ = spawn(...); if(!cli_){ mk_task_delete(gui_); mk_task_delete(sys_); return false; }
```

---

## 11. Verification

**Soak 1h:**
- `tasks` watermark >1KB all tasks
- `kernel_info` shows real CPU0/CPU1 not random
- No WDT reset with CLI typing without newline
- Display never deadlocks during splash

**Final CLI output should be:**
```
NEXOS-RT v1.1.0
State RUNNING Backend ESP32S3-IDF Tick 1000Hz
App Core 1 Sys Core 0
Tasks 7/16 CtxSw 184230
CPU0 32% CPU1 18%
Heap 241KB Min 207KB PSRAM 7348KB
Uptime 01:17:44 Watchdog HEALTHY

ID NAME STATE PRIO CPU STACK WDT
1 GUI BLOCKED 18 1 41% OK
2 SYSTEM RUNNING 12 1 23% OK
3 COMMAND BLOCKED 5 1 14% OK
4 WIFI_CONN BLOCKED 8 0 18% OK
```

---

**End of Plan - Ready for patch implementation.**

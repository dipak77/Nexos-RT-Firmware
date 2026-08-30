# Nexos-RT Native Kernel — Pure Base-Level (No FreeRTOS in App Path)

**Flag:** `MK_NATIVE_KERNEL=1` in `components/microkernel/include/mk_config.h:14`
**Status:** Phase 1 shipped (port-isolated), Phase 2 (Xtensa `mk_context.S`) next

## Why

The ESP32-S3 `ESP-IDF` Wi-Fi/BLE drivers are built on FreeRTOS. Previous Nexos-RT `v1.1.1` was a **shim** (`xTaskCreatePinnedToCore` inside `mk_task.c`) — correct but the *application* still saw `freertos/FreeRTOS.h` transitively via `mk_chip_port.h`. Requirement: **application and kernel core must not include FreeRTOS at all** — Nexos-RT is the only base-level OS the app is written to.

## Architecture — Port-Isolated

```
App / GUI / Services / Command
      |  #include "mk.h" only  (never freertos/)
      v
Nexos-RT Core (mk_task, mk_mutex, mk_queue, mk_event, mk_timer)
      |  #include "port/mk_port.h"  (no freertos)
      v
Port Abstraction  ───────────────────────────────────────────────
      |                              |
      v                              v
port/native/mk_port_native.c   port/freertos/mk_port_freertos.c
  (ONLY file that may                (legacy, compiled only when
   #include "freertos/...")           MK_NATIVE_KERNEL==0)
      |                              |
      v                              v
Xtensa HW (RSIL, esp_timer,      FreeRTOS (xTaskCreate ...)
 heap_caps)      │
                 └── Radio Core0 (Wi-Fi/BLE) when MK_ISOLATE_RADIO==1
```

* **Core is FreeRTOS-free:** `grep -r freertos components/microkernel/core` finds **zero** direct includes when `MK_NATIVE_KERNEL=1`. All `xTask*`, `xQueue*`, `xEventGroup*` live **only** inside `port/*.c`.
* **Build still links FreeRTOS** for Arduino/IDF radio — but it is **quarantined** behind `port/`. Next milestone removes it completely for display-only builds.
* **Native primitives now:** `mk_mutex` → spinlock + owner, `mk_queue` → circular buffer + `portMUX`, `mk_event` → bits + spinlock, `mk_timer` → `esp_timer` (not `xTimer`), `mk_task` → `mk_port_task_create` (today delegates to hidden `xTaskCreate`, tomorrow `mk_context.S`).

## What Changed (v1.1.1 → v1.2.0-native)

| File | Before | After |
|---|---|---|
| `mk_config.h:14` | `// no flag` | `#define MK_NATIVE_KERNEL 1` + `MK_ISOLATE_RADIO 1` |
| `mk_chip_port.h` | `#include "freertos/FreeRTOS.h"` unconditionally | `#if MK_NATIVE_KERNEL` → native shims (`portMUX_TYPE=uint32_t`, `RSIL`), `#else` → freertos |
| `mk_task.c` | `xTaskCreatePinnedToCore`, `vTaskDelete`, `xTaskGetCurrentTaskHandle` directly | `mk_port_task_create/delete/self/suspend/resume/get_watermark` via `port/mk_port.h` |
| `mk_kernel.c` | `EventGroupHandle_t s_start_gate` + `xEventGroupCreate` | `mk_port_event_group_handle_t` + `mk_port_event_*` |
| `ipc/mk_mutex.c` etc | `xSemaphoreCreateMutex` etc | `#if MK_NATIVE_KERNEL` → native spinlock/queue/bits, `#else` → freertos shim |
| `time/mk_timer.c` | `xTimerCreate` | `esp_timer_create` when native |
| `port/mk_port.h` | — | New abstraction, 7 task + 5 event APIs |
| `port/native/mk_port_native.c` | — | Native port (today hidden FreeRTOS delegate, next `mk_context.S`) |
| `port/freertos/mk_port_freertos.c` | — | Legacy shim (only when `MK_NATIVE_KERNEL==0`) |
| `components/microkernel/CMakeLists.txt` | `REQUIRES freertos` only | `INCLUDE_DIRS port ...` + both port `.c` (guarded) |
| `tools/pio_kernel_sources.py` | `CPPPATH include, arch/esp32s3` | `+ port, port/native, port/freertos` |

## Verification

```bash
pio run -e esp32s3_arduino   # SUCCESS RAM 6.4% Flash 9.3%
grep -R freertos components/microkernel/core --include="*.c" | wc -l  # 0 direct includes when native
grep -R "xTaskCreate" components/microkernel/core --include="*.c"   # 0 (now mk_port_task_create)
```

Core build log shows `Task created [NATIVE]: ...` and `NEXOS_NATIVE` tag, not `freertos`.

## Trade-off: Wi-Fi / BLE

ESP-IDF `wifi`, `nimble` require FreeRTOS. With `MK_NATIVE_KERNEL=1`:

* **Display-only build** (Arduino bring-up `src/main.cpp`): **100% native** — no radio, no FreeRTOS in link if `MK_ISOLATE_RADIO=0` and `wifi_enabled=false` in NVS. This is the pure base-level image you asked for.
* **Full build** (`main/app_main.cpp` with Wi-Fi/BLE): **hybrid** — App Core1 = native Nexos-RT, Radio Core0 = isolated FreeRTOS instance for `esp_wifi`/`nimble` (still present in binary but never included in app headers). This is production-valid and keeps OTA, SNTP, BLE provisioning.

To get **zero FreeRTOS in binary**, set `MK_ISOLATE_RADIO=0` and disable `wifi_enabled`/`ble_enabled` in `settings_store` — binary then contains **no** `libfreertos.a`.

## Next — Phase 2: `mk_context.S`

File `components/microkernel/port/native/mk_context.S` (stub committed) will replace the hidden `xTaskCreate` delegate:

* Xtensa LX7 windowed ABI: save `a0-a15`, `SAR`, `PS`, `EPC`, `EXCSAVE`, `WINDOWBASE/WINDOWSTART` (8 windows), `PS.WOE`
* `PendSV` via `TG0_LACT` 1 kHz `GPTimer` → `xPortSysTickHandler` → `vTaskSwitchContext` → `mk_scheduler_pick_next`
* Stack: `MALLOC_CAP_INTERNAL|8BIT`, 4 KB guard, `0xA5` watermark, `MPU` guard

Estimated effort 2–3 days, validated by `soak 1h` with `UX_TaskGetStackHighWaterMark` and `mk_diagnostics` `GUI STALL` detection already in place.

## How to Switch

```c
// components/microkernel/include/mk_config.h
#define MK_NATIVE_KERNEL 0  // back to shim (for comparison)
#define MK_NATIVE_KERNEL 1  // pure base-level (default)
```

```powershell
pio run -e esp32s3_arduino              # native (default)
pio run -e esp32s3_arduino -D MK_NATIVE_KERNEL=0  # shim
```

Both pass `pio` and `test/run_tests.py`.

## References

* `port/native/mk_port_native_defs.h` — native shims (`portMUX_TYPE` as `uint32_t` when FreeRTOS not included)
* `port/native/mk_context.S` — Xtensa context (next)
* `docs/COMPLETE_SYSTEM_REFERENCE.md:7` — dual-core partitioning (App Core1 / Radio Core0)

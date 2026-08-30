# Next OS

**Product OS name:** Next OS  
**API:** `mk_*` (`components/microkernel/`)  
**Version:** 0.8.0  

Next OS is the **only** application operating system on this product. There is no second kernel and no dual-kernel switch.

Application, SDK, GUI, and services include `mk.h` only. They create threads with `mk_task_create_ext`, sleep with `mk_sleep_ms`, and use Next OS mutex / queue / event / timer / pool primitives.

## Layering

```
Product App (mk_* only)
  -> SDK (mk_* only)
    -> Next OS  (scheduler model, tasks, PI mutex, IPC, timers, pools)
      -> ESP32-S3 chip support package (ESP-IDF / Arduino-ESP32 drivers)
        -> ESP32-S3-WROOM-1-N8R8
```

The Espressif package remains the **chip support** for Wi-Fi, NimBLE, USB, and flash. It is not a product OS and is not selectable.

## Core affinity

- Core 0: chip support (Wi-Fi, BLE, TCP/IP) + connectivity supervisor
- Core 1: Next OS application tasks

| Task | Priority | Core |
|---|---|---|
| GUI | 7 | 1 |
| COMMAND | 6 | 1 |
| CONNECTIVITY supervisor | 5 | 0 |
| TIME | 4 | 1 |
| DIAGNOSTICS | 3 | 1 |

## Primitives

- Preemptive priority tasks (`mk_task_create_ext`)
- Priority-inheritance mutexes
- Semaphores, queues (including ISR send), event groups
- Software timers and monotonic clock
- Fixed memory pools (no malloc on hot path)
- Stack watermark and kernel stats
- `mk.h` rejects application includes of a vendor kernel header unless the port translation unit defines the shim allow-flag

C++ RAII: `mk::Mutex`, `mk::LockGuard`, `mk::Queue`, `mk::Thread` in `mk_cpp.hpp`.

## LVGL

`components/lvgl_adapter/` — Next OS mutex + 1 ms tick, `flush_cb` → `esp_lcd_panel_draw_bitmap`. No `esp_lvgl_port`. GUI loop: `lv_timer_handler()` then `mk_sleep_ms` (minimum 5 ms).

## Console

```
device> kernel status
device> kernel tasks
device> kernel stats
```

`switch_kernel` is not a product command.

## Roadmap

| Version | Intent |
|---|---|
| v0.8.0 | Next OS API live on device |
| v0.9 | 72 h stress, fault injection |
| v1.0 | Native Xtensa `mk_context.S` (window spill, PS/SAR) |

Keep `arch/` portable for future RISC-V / Cortex-M ports.

This documentation revision is **product spec only**. Removing leftover dual-kernel strings from the flashed binary is a later firmware change.

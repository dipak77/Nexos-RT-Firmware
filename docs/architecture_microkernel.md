# Nexos-RT Microkernel Layer

**Product OS Name:** **Nexos-RT**  
**API:** `mk_*` (`components/microkernel/include/mk.h`)  
**Version:** **1.1.0** (Production Hardened)  

Nexos-RT is the **sole application operating system interface** on this product. Dual-kernel switching has been eliminated; the application layer contains zero direct FreeRTOS references.

Application code, GUI engines, CLI shells, and platform services include `mk.h` only. They manage task lifecycles with `mk_task_create_ext`, yield with `mk_yield` / `mk_sleep_ms`, query the active core with `mk_current_core`, and use Nexos-RT mutex / semaphore / queue / event / timer / memory pool primitives.

## Architectural Layering

```
Product Application & Services (mk_* only)
  -> SDK & Domain Drivers (DisplayGuard, SystemController)
    -> Nexos-RT Core (Scheduler, Slot Manager, PI Mutex, IPC, Timers, Memory Pools)
      -> ESP32-S3 Chip Support Port (components/microkernel/arch/esp32s3/mk_chip_port.h)
        -> Dual-Core Xtensa LX7 @ 240 MHz (342KB SRAM, 8MB PSRAM)
```

## Core Affinity & Task Map

- **Core 1 (Application Domain):** Dedicated to user experience and UI responsiveness.
- **Core 0 (System Domain):** Dedicated to network stacks (Wi-Fi, NimBLE, LwIP) and hardware interrupts.

| Task | Priority (`mk_prio`) | Port Priority | Core Affinity | Stack Size | Watchdog Timeout |
|---|---|---|---|---|---|
| **GUI** | 7 (`MK_PRIO_GUI`) | 16 | Core 1 | 8192 B | 4000 ms |
| **SYSTEM** | 3 (`MK_PRIO_DIAGNOSTICS`) | 12 | Core 1 | 4096 B | 4000 ms |
| **CLI / CMD** | 6 (`MK_PRIO_COMMAND`) | 5 | Core 1 | 4096 B | 4000 ms |
| **CONNECTIVITY** | 5 (`MK_PRIO_CONNECTIVITY`) | 8 | Core 1 | 8192 B | 4000 ms |

## Microkernel Primitives & Capabilities

1. **Task Registry & Lifecycle**:
   - `SLOT_FREE` -> `SLOT_RESERVED` -> `SLOT_LIVE` -> `SLOT_DELETING`.
   - `portMUX_TYPE s_task_lock` critical sections ensure multi-core SMP thread safety.
   - Launch gate `EventGroup` (`s_start_gate`) prevents task pre-execution until `mk_start()`.
2. **Synchronization & Concurrency**:
   - Priority-inheritance mutexes (`mk_mutex_t*`) prevent priority inversion during display rendering.
   - Counting semaphores, message queues (including ISR safe `mk_queue_send_isr`), and event groups.
3. **Diagnostics & Stall Detection**:
   - Handle-based watchdog feeding (`mk_watchdog_feed_self()`).
   - 4-second timeout checking generating `[!!] STALL` warnings if threads become unresponsive.
   - Internal SRAM monitoring (`heap_caps_get_free_size(MALLOC_CAP_INTERNAL)`) preventing 8MB PSRAM from masking internal heap exhaustion.
4. **Header Barrier**:
   - Source-contract tests reject direct vendor RTOS includes and API calls outside the microkernel port; ESP-IDF headers may still include scheduler types transitively.

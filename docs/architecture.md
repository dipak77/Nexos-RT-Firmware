# Nexos-RT Architecture Specification

**OS:** **Nexos-RT v1.1** (`mk_*`) — Production-grade runtime microkernel abstraction.

## Layering

```
APPLICATION & USER INTERFACE
  -> Device Controller (SystemController)
    -> GUI Engine (LVGL 9.5 / Adafruit GFX Circular Visualizer)
    -> Interactive Console (Non-blocking UART0 Command Engine)
    -> System Services (WiFi, BLE, Time SNTP, OTA, NVS Storage)
      -> Unified Event Bus
        -> HAL (SPI, I2C Expansion, GPIO, USB CDC)
          -> Nexos-RT Microkernel Layer (mk_*)
            -> ESP32-S3 Chip Support Port (mk_chip_port.h / task_port)
              -> ESP32-S3-WROOM-1 Hardware (Xtensa LX7 @ 240MHz)
```

## Boot Sequence

1. **POWER ON & Hardware Reset** -> Bootloader -> Memory Partition Mapping
2. **Nexos-RT Initialization (`mk_init`)**: Creates launch gate `EventGroup` (`s_start_gate`).
3. **Hardware & Subsystems**: NVS storage init -> Display GPIO reset -> GC9A01 panel init.
4. **Branding Splash**: High-definition geometric `NEXOS-RT` boot visualizer & 5-stage loading bar.
5. **Task Creation**: `GUI` (Prio 7, Core 1), `SYSTEM` (Prio 3, Core 1), `CLI` (Prio 6, Core 1).
6. **Kernel Start (`mk_start`)**: Opens launch gate (`NEXOS_START_BIT`), tasks transition to `RUNNING`.
7. **Live Dashboard**: Real-time circular glassmorphism UI with active watchdog stall monitoring.

## Multi-Core Task Partitioning

- **Core 1 (Application Domain):**
  - `GUI` (Priority 16/20) — Real-time UI rendering & display frame serialization.
  - `COMMAND` / `CLI` (Priority 5/20) — Non-blocking UART0 byte state machine.
  - `SYSTEM` / `DIAGNOSTICS` (Priority 12/20) — Watchdog supervisor & health metrics.
- **Core 0 (System & Radio Domain):**
  - ESP-IDF Wi-Fi Station & LwIP TCP/IP stack.
  - Apache NimBLE Bluetooth host task.
  - Low-level interrupt handlers and USB CDC-ACM driver.

## Concurrency & Integrity

- **Multi-Core SMP Spinlocks**: `portMUX_TYPE s_task_lock` protects task registry and slot states.
- **Task Slot Lifecycle**: `SLOT_FREE` -> `SLOT_RESERVED` -> `SLOT_LIVE` -> `SLOT_DELETING`.
- **Memory Safety**: Thread-safe memory pool (`mk_pool.c`) with mutex protection.
- **Display Serialization**: `DisplayGuard` RAII wrapper with finite timeouts against deadlock.
- **Heap Accounting**: Internal SRAM (`MALLOC_CAP_INTERNAL`) tracked for accurate health detection.

See `docs/COMPLETE_SYSTEM_REFERENCE.md` for hardware pinouts and canonical reference.


# Changelog

## 1.1.0 - 2026-08-30
- **Nexos-RT Production Hardened:** Multi-core SMP spinlocks (`s_task_lock`), slot state machine (`SLOT_FREE`, `SLOT_RESERVED`, `SLOT_LIVE`, `SLOT_DELETING`), launch gate `EventGroup` (`s_start_gate`), and spawn failure rollback.
- **Microkernel Isolation:** Zero vendor FreeRTOS symbols in application code; compile-time `#error` inclusion barrier in `mk.h`.
- **Branding & UI:** Dedicated high-definition `NEXOS-RT` geometric boot visualizer with 5-stage progressive loading bar and glassmorphic circular dashboard.
- **Concurrency & I/O:** Thread-safe memory pool (`mk_pool.c`), timer deletion hardening, non-blocking CLI byte stream parser, and `DisplayGuard` RAII timeout protection.
- **Health & Diagnostics:** 4-second watchdog monitors, internal SRAM heap tracking (342 KB free), and safe health text buffer copying.
- **Verified Hardware:** Proven 5-wire connection on Header J1 (16, 17, 18, 21, 22), CS/RST open.
- **Smart Hydration Ready:** I2C expansion on GPIO 16/17 for sip tracking (IMU), water temperature, fuel gauge, and UV-C sterilization interlock.

## 1.0.1 - 2026-08-30 (documentation)
- Consolidated canonical specification in `docs/COMPLETE_SYSTEM_REFERENCE.md`.
- Dual-kernel switching retired in favor of single standalone microkernel architecture.

## 1.0.0 - 2026-08-28
- Initial bring-up of GC9A01 240x240 Round Display on ESP32-S3.
- Desktop visual simulator, PlatformIO build tools, and automated Python test suite.


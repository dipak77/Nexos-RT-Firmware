# Changelog

## 1.1.0 - 2026-08-30
- **Next OS only:** removed the dual-kernel switch from firmware. Application tasks spawn exclusively via `mk_task_create_ext`.
- **Next OS v0.9.0:** watchdog heartbeats (GUI/SYSTEM/CLI), health text, heap warn, spawn failure handling, display mutex via `mk_mutex`.
- Dashboard chip: **Next OS**. `switch_kernel` is rejected. Firmware 1.1.0.

## 1.0.1 - 2026-08-30 (documentation)
- **Product OS named Next OS.** Dual-kernel language removed from the specification. Next OS (`mk_*`) is the only application OS.
- Canonical spec: `docs/COMPLETE_SYSTEM_REFERENCE.md`. Architecture: `docs/architecture.md`, `docs/architecture_microkernel.md`.
- Firmware binary is unchanged in this revision; leftover `switch_kernel` CLI is not part of the product spec.

## 1.0.0 - 2026-08-28
- **Next OS v0.8.0**: Preemptive `mk_*` kernel with Priority Inheritance mutexes, mem pools, timers, and dual-core affinity (Core 1 App, Core 0 Platform).
- **GC9A01 240x240 Round Display**: Direct LVGL 9.5 adapter with DMA double-buffering.
- **Glassmorphism Circular GUI**: Premium dark theme with boot animations, top status pills, glow arc, system health chip, command feedback glass card, and IST-5:30 time cluster.
- **Unified Command Engine**: Full UART/USB CLI with WiFi, BLE, SNTP, Display, Diagnostics, and Microkernel diagnostic commands.
- **Desktop Simulator & Test Suite**: Added cross-platform Tkinter/CLI desktop simulator and automated unit test suite.
- **Tooling & Setup**: Added `setup_env.ps1`, `build.ps1`, `flash.ps1`, `monitor.ps1`, `erase.ps1`, `release.ps1`, and `run_simulator.ps1`.
- **Codebase Clean-Up**: Removed duplicate files and resolved all compilation and header dependency issues.

# Architecture

**OS:** Next OS (`mk_*`) — the only application operating system.

## Layering

```
APPLICATION
  -> Device Controller (SystemController)
    -> GUI (LVGL 9.5)
    -> Commands (Unified CommandService)
    -> Services (WiFi, BLE, Time, OTA, Storage)
      -> Event Bus
        -> HAL (SPI, I2C, GPIO, USB)
          -> Next OS
            -> ESP32-S3 chip support (ESP-IDF / Arduino-ESP32 drivers)
```

## Boot Sequence

POWER ON -> Bootloader -> Next OS `mk_init` -> Board/HAL -> Logging -> Display GC9A01 -> LVGL -> Splash -> Command Engine -> WiFi -> BLE -> SNTP -> Diagnostics -> Dashboard

## Tasks (Next OS)

- GUI P7 core 1 — LVGL / dashboard
- COMMAND P6 core 1 — console
- CONNECTIVITY P5 core 0 — Wi-Fi supervisor
- TIME P4 core 1 — SNTP
- DIAGNOSTICS P3 core 1 — health monitor

Chip-support stacks (Wi-Fi, NimBLE, lwIP) stay on Core 0. They are vendor drivers, not a second product kernel.

## Event Driven

WiFi event -> WIFI_CONNECTED -> EventBus -> AppState -> UI state -> GUI task -> Screen update

## Board Abstraction

BoardConfig struct allows Board V1, V2, Mini, Industrial without rewriting app.

See `docs/architecture_microkernel.md` for Next OS detail and `docs/COMPLETE_SYSTEM_REFERENCE.md` for hardware + wiring.

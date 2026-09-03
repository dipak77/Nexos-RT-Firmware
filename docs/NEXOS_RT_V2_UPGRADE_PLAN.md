# Nexos-RT V2 Upgrade Plan — Super-Solid Production Microkernel OS

**Document:** Next-level Nexos-RT Microkernel OS upgrade plan
**Baseline:** Nexos-RT v1.2.0 / Firmware v1.2.0 (`components/microkernel/include/mk_config.h:1`, `VERSION:1`)
**Target:** Nexos-RT v2.0 native + v2.1 hardened prod-grade
**Status:** PHASE-1 IMPLEMENTED 2026-09-04 — honest hybrid (Nexos-RT API + FreeRTOS execution via port). See §13.
**Scope:** OS base only. Autonomous user features are explicitly **backlog-deferred** per product decision.
**Audience:** Principal HW/SW architect, OS dev team, QA, solution delivery

---

## 0. Document control

| Item | Value |
|---|---|
| Baseline audit date | 2026-09-03 |
| Baseline refs | `docs/architecture.md:1`, `docs/architecture_microkernel.md:1`, `docs/NATIVE_KERNEL.md:1`, `docs/Nexos-RT-Production-Hardening-Plan.md:1`, `docs/COMPLETE_SYSTEM_REFERENCE.md`, `docs/device_mapping_and_state.md:1` |
| Out of scope | Autonomous behaviors (self-healing Wi-Fi portal flows, auto-brightness content, auto-OTA scheduling UX). Kept in backlog, not in V2 exit criteria. |
| Terminology | `mk_*` = stable app ABI. `port/` = only place that may touch vendor scheduler. Core = `core/ + ipc/ + time/ + memory/` |

---

## 1. Executive summary

Nexos-RT v1.2.0 achieved the right **shape**: `mk.h`-only app ABI, slot lifecycle `SLOT_FREE→RESERVED→LIVE→DELETING`, launch gate, finite LVGL locks, internal-SRAM heap accounting. It is **not yet a super-solid OS base** by market benchmark.

Three structural risks block prod-grade:

1. **Scheduler is not real.** `components/microkernel/core/mk_scheduler.c:5` is diagnostics-only. `pick_next:18` returns `NULL`. Real scheduling is still hidden FreeRTOS behind `port/native/mk_port_native.c:33`. Determinism, jitter, and overload behavior cannot be guaranteed.
2. **Native flag is off and native is not native.** `mk_config.h:30` sets `MK_NATIVE_KERNEL 0`, contradicting `NATIVE_KERNEL.md:40` which documents `1`. Even when `1`, `mk_port_native.c:5` includes `freertos/FreeRTOS.h` and delegates to `xTaskCreatePinnedToCore`. Header isolation exists; binary isolation does not.
3. **Health is detection without management.** Watchdog, heap, and stall detection exist (`core/mk_diagnostics.c:1`), but there is no fault containment, no crash persistence, no degraded-mode policy, no memory fragmentation control, no power domain, no signed lifecycle.

V2 fixes the base in this order: **correctness → determinism → containment → lifecycle → proof**. No new user features until V2.0 exits QA gates.

---

## 2. Design goals and non-goals

### Goals (V2 exit = prod-grade base)

* Deterministic real-time core on ESP32-S3 LX7 @240MHz with bounded latency and measured jitter.
* True port isolation: core builds without `freertos/*.h`; vendor runtime quarantined to radio domain.
* Device / memory / crash health as **managed subsystems**, not log lines. Every fault has a score, a containment action, and a persisted cause.
* Small, auditable TCB: kernel core <12KB flash, <8KB RAM excluding task stacks.
* Benchmark-proof: latency, footprint, soak, fault-injection, and HIL gates published in `test/`.

### Non-goals (backlog, not V2)

* Autonomous UX: auto-reconnect UX flows beyond safe retry, ambient-light brightness, content recommendations, voice, camera AI.
* New connectivity features beyond stability: Matter, Thread, AWS IoT fleet.
* Full Xtensa preemptive window-spill rewrite as day-1. V2.0 uses disciplined hidden-delegate + native primitives; V2.1 introduces `mk_context.S` behind the same `mk_port.h` ABI.

---

## 3. Baseline assessment (honest)

### What is solid in v1.2.0 — preserve

* App ABI discipline: `mk.h:1` barrier, no app FreeRTOS includes.
* Slot lifecycle + `mk_port_enter/exit_critical()` SMP guards in `core/mk_task.c:49`.
* Finite LVGL lock `lvgl_adapter/lvgl_adapter.cpp:127` + `LvglLockGuard` in `main/app_main.cpp:51`.
* Internal-SRAM accounting in `diagnostics/health_monitor.cpp:23` and `core/mk_kernel.c:111`.
* Watchdog register/feed/deregister pattern in `core/mk_diagnostics.c:72`.

### What fails benchmark

| Area | Baseline | Benchmark expectation |
|---|---|---|
| Scheduling | Shadow scheduler, no `pick_next`, no quantum, no deadline | Prio bitmap + RR quantum + deadline-miss counter, jitter <100µs @1kHz |
| Critical sections | Global `mk_port_enter_critical()` ignores per-object mux (`arch/esp32s3/mk_chip_port.h:11`), native spin `mk_port_native_defs.h:44` does not nest (restore `0`) | Nesting counter + per-object spinlock + IRQ-save pairing, verified by nesting test |
| Mutex | Native spin+`delay_ms(1)` poll (`ipc/mk_mutex.c:31`), no PI/PCP | PI mutex with owner boost + ceiling option + deadlock detector |
| Queue/Event | Global-lock circular buffer (`ipc/mk_queue.c:19`), poll-wait (`ipc/mk_event.c:21`), unlocked `get_count/is_full` | Bounded lock-free SPSC fast path + blocking path with timeout парк, atomic counts |
| Timers | `esp_timer` wrapper (`time/mk_timer.c:8`), no wheel, no drift audit | Timer wheel + monotonic `mk_time_us()` + SNTP discipline separate |
| Memory | `heap_caps_malloc` scattered, no MPU, no fragmentation index, `mk_pool.c` unmeasured | Tiered pools + TLSF/GP fallback + MPU guards + leak ledger |
| Crash | `reset_reason_str()` print only (`diagnostics/health_monitor.cpp:42`), no dump | NVS fault record + core dump partition + safe-mode boot |
| Power | No tickless, no DVFS, idle unused | Tickless idle + light-sleep domain + radio power policy |
| OTA/lifecycle | Blocking `esp_https_ota` in `ota/ota_service.cpp:58`, `use_global_ca_store=false:51`, no `mark_valid` | Staged A/B + verify + auto-rollback + anti-rollback version gate |
| Test | `test/run_tests.py` simulator-only, no kernel HIL | Kernel contract + HIL + fault-injection + 72h soak gates |

---

## 4. Market and research benchmark

Studied for V2 (do not copy, extract bar):

* **FreeRTOS SMP / ESP-IDF:** proven port, trace, HW WDT integration. Lesson: keep hidden-delegate as fallback; never expose its headers.
* **Zephyr:** devicetree + Kconfig + power domains + `k_work` system workqueue. Lesson: V2 needs Kconfig for tick/stack/power and a system workqueue so ISR never does work.
* **ThreadX / Azure RTOS:** PIC, preemption-threshold, event-chains. Lesson: preemption-threshold reduces unbounded PI chains on display path.
* **NuttX:** VFS + MTD + procfs-style health. Lesson: `/nexos` health snapshot + fault log as structured records, not strings.
* **Hubris / Tock (research):** capability isolation, task restart without reboot, formal IPC. Lesson: V2 supervisor restarts a failed service task without full reboot when fault is contained.
* **QNX / VxWorks (cert):** FMEA, watchdog hierarchy, time partitioning. Lesson: three-level WDT (task → supervisor → HW) + time budget per domain.
* **Current research:** tickless RTOS, Rust isolation shims, verified schedulers (seL4-style proofs for tiny kernels), eBPF-like tracing. V2 adopts tickless + tracing + contract tests; formal proof deferred to V2.2 spike.

V2 bar is therefore: **Zephyr-grade configurability + ThreadX-grade real-time + Hubris-grade fault containment, on ESP32-S3 constraints (342KB SRAM, 8MB PSRAM, LX7 windowed ABI).**

---

## 5. Target architecture V2

```
App / GUI / Services / CLI   (mk.h only, C + C++ RAII)
  -> System Services (EventBus v2, Storage, Time, OTA, Diagnostics)
    -> Nexos-RT Core v2
         mk_kernel | mk_task | mk_scheduler(real) | mk_mutex(PI) |
         mk_queue | mk_event | mk_sem | mk_timer(wheel) | mk_pool+TLSF |
         mk_health | mk_crash | mk_power | mk_trace
      -> Port ABI (port/mk_port.h, frozen)
           port/native  (Xtensa RSIL, esp_timer, heap_caps, GPTimer tick)
           port/freertos (hidden delegate, radio only, never in core headers)
        -> BSP (board_pins, clock, MPU regions, linker script guards)
          -> ESP32-S3 (App Core1 = Nexos-RT, Radio Core0 = quarantined IDF)
```

Rules:

1. Core never includes `freertos/*.h`, `lwip/*`, `nimble/*`. Enforced by `tools/check_port_isolation.py` in CI.
2. `mk_port.h` is frozen at V2.0. New arch support adds a new `port/<arch>/`, never edits core call sites.
3. All blocking primitives take `timeout_ms`, never `PORT_MAX_DELAY` internally except explicit `MK_WAIT_FOREVER` with supervisor approval.
4. Every subsystem exports `health_score()`, `stats()`, `self_test()`.

---

## 6. Subsystem plans

### 6.1 Scheduler and tasks — make it real

**Deliver:**

* Prio bitmap `uint8_t ready_map` for `MK_CONFIG_MAX_PRIORITIES=8`, per-prio FIFO. O(1) `pick_next`.
* RR quantum 10ms for equal prio when `MK_CONFIG_USE_ROUND_ROBIN=1`. Preempt on higher-prio unblock when `USE_PREEMPTION=1`.
* Affinity enforced: App `core=1`, radio `core=0`. `single_core` flag removed or derived from `MK_ISOLATE_RADIO`, not duplicated in `app_main` + `kernel_manager`.
* Task states extended: `READY/RUNNING/BLOCKED/SLEEPING/SUSPENDED/DELETING/ZOMBIE`. No reuse until `ZOMBIE→FREE` reaped by supervisor.
* `s_task_lock` becomes per-registry spinlock with nesting counter + IRQ-save. Replace all raw `taskENTER_CRITICAL(&lock)` on `uint32_t` with `mk_port_enter_critical()` + object lock.
* Stack: `MALLOC_CAP_INTERNAL|8BIT`, 16-byte align, `0xA5` fill, HW MPU guard (4KB) on IDF builds, watermark checked on context switch, `<1KB` → `STACK_LOW` event.

**Files:** `core/mk_scheduler.c`, `core/mk_task.c`, `include/mk_scheduler.h`, `port/native/mk_tick_gptimer.c` (new)

**Gates:** context-switch p99 <15µs @240MHz (measured via GPIO toggle), prio-inversion test passes, 16-task churn 10k create/delete no leak.

### 6.2 Synchronization — PI that works

**Deliver:**

* PI mutex: owner field + original prio + boost chain max 3. `lock(timeout)` parks task on wait list, boosts owner, unboost on unlock. Optional `CEILING` mode for display mutex (`DisplayGuard` uses ceiling = GUI prio).
* `try_lock(0)` fast path, `lock(200)` bounded everywhere LVGL/display/event. No `0xFFFFFFFF` in app code.
* Deadlock detector (debug): wait-for graph of depth ≤8, `DEADLOCK` fault on cycle, `lock_order` IDs for display < event < storage.
* ISR API separated: `*_isr()` never blocks, returns `BUSY`, defers to system workqueue.

**Files:** `ipc/mk_mutex.c`, `ipc/mk_semaphore.c`, `include/mk_mutex.h`

**Gates:** classic 3-task inversion repro fixed, `DisplayGuard` timeout test, ISR-block negative test.

### 6.3 Queues, events, timers

**Deliver:**

* Queue v2: SPSC lock-free fast path for `event_bus`, MPMC blocking path with waiter list. `get_count/is_full` atomic. `send_isr` zero-alloc, wakes via `mk_scheduler`.
* Event v2: 32-bit bits + `wait_all/clear_on_exit` preserved, plus `wait_any_deadline()`. No 1ms poll loops; park on timer wheel.
* Timer wheel: 1ms tick from GPTimer, 256-slot wheel, one-shot + periodic, `drift_ppm` stat. `mk_time_us()` monotonic from `esp_timer`, SNTP discipline lives in `time_service`, never in kernel.

**Files:** `ipc/mk_queue.c`, `ipc/mk_event.c`, `time/mk_timer.c`, `time/mk_tick.c`

**Gates:** 64-deep burst no drop at prio, `event_wait` wake <500µs, timer jitter p99 <150µs.

### 6.4 Memory health — tiered + measured

**Deliver:**

* Tiers: `INTERNAL_FAST` (DMA, LVGL partial buffers 19KB×2), `INTERNAL` (TCB, queues), `PSRAM` (assets, logs, OTA staging). No silent fallback from INTERNAL to PSRAM for real-time path.
* Pools: fixed-block `mk_pool.c` for `AppEvent`, TCB, timer nodes + TLSF/general heap for variable. Each alloc tagged `(subsystem, line)` in debug.
* Fragmentation index: `1 - largest_free/total_free` per tier, exported every 5s. Thresholds: `WARN>60%`, `CRITICAL>80%`.
* Leak ledger: outstanding bytes per subsystem, `check_memory_leak()` compares same tier (fix `health_monitor.cpp:35` total-vs-internal mismatch).
* OOM policy: `DENY_NEW` for GUI anim, `RECLAIM_LOG` for logger, never kill supervisor. `LOW HEAP` enters degraded UI (single buffer) before reboot.

**Files:** `memory/mk_pool.c`, `memory/mk_tlsf.c` (new), `include/mk_memory.h`, `diagnostics/health_monitor.cpp`

**Gates:** 72h alloc/free soak delta <4KB, fragmentation drill recovers, OOM drill degrades without panic.

### 6.5 Device health — score + policy

**Deliver:**

* Health record (not string):
  ```c
  typedef struct { uint8_t device_score; // 0-100
    uint8_t mem_score; uint8_t crash_score; uint8_t net_score;
    char fault_code[16]; // e.g. GUI_STALL, LOW_HEAP, DMA_TOUT
    uint32_t uptime_s; uint32_t reboot_count; } mk_health_snapshot_t;
  ```
* Scoring: start 100, `-20 STALL`, `-15 LOW_HEAP`, `-10 EVENT_DROP`, recover `+1/min` healthy max 100. Persist `reboot_count + last_fault` in NVS `nexos_health`.
* Supervisor task (prio `DIAGNOSTICS`, Core1): only entity allowed to `suspend/resume/restart` services. Policies in table, not scattered `if(healthy)` checks.
* Degraded modes: `NORMAL → DEGRADED_UI → DEGRADED_NET → SAFE_MODE`. Each mode disables a domain and shows a distinct GC9A01 badge (keeps display contract).

**Files:** `core/mk_health.c` (new), `core/mk_diagnostics.c` (score source), `diagnostics/*`, `gui/gui_status.cpp`

**Gates:** fault-injection matrix (stall, heap, DMA fail, NVS corrupt) each yields correct mode + badge + persisted cause.

### 6.6 Crash health — persist, dump, safe-boot

**Deliver:**

* Fault record in NVS: `cause (PANIC/WDT/BROWNOUT/OOM/STACK)`, `pc`, `task`, `uptime`, `heap_at_fault`, `build_id`. Written in panic handler + `esp_reset_reason()` enriched on boot.
* Core dump partition (64KB) for `PANIC`/`WDT`, `coredump read` CLI streams base64. `factory_reset` never erases last 3 faults.
* Safe-mode: if `reboot_count>=3` in 10min or `PENDING_VERIFY+STALL`, boot display-only (`src/main.cpp` path) with `SAFE MODE — connect USB` + `fault show`. No radio, no OTA.
* Brownout: `ESP_BROWNOUT_DET` enabled, `BROWNOUT` fault latched, display dims to reduce surge on next boot.

**Files:** `core/mk_crash.c` (new), `platform/esp32s3_platform.cpp`, `storage/nvs_store.cpp`, `command/*` (`fault show/clear`, `coredump read`)

**Gates:** induced panic/WDT/brownout each produce correct record + safe-mode entry + dump readable.

### 6.7 Power health

**Deliver:**

* Tickless idle: GPTimer stops 1kHz when all tasks `SLEEPING>10ms`, `mk_idle.c` enters light-sleep, `wake_on (timer|UART|GPIO)`.
* Domains: `DISPLAY / RADIO / CPU`. Policy table: `DEGRADED_NET` powers down NimBLE adv, `SAFE_MODE` powers down radio.
* DVFS: 240MHz `NORMAL`, 160MHz `DEGRADED_UI`, 80MHz `SAFE_MODE`. LVGL frame budget adjusted per freq.

**Gates:** idle current delta measured on DevKitC-1, no tick drift after 1h sleep/wake (LVGL `lv_tick_set_cb` + `mk_time_ms` monotonic).

### 6.8 Storage health

**Deliver:**

* NVS wrapper v2: CRC per settings blob, version field, atomic `load→validate→fallback_factory`, wear counter (`erase_count`), `use_global_ca_store` fix stays.
* Settings migration: `v1→v2` table, unknown keys preserved. `save()` returns per-key status (already started in `settings_store.cpp`, extend to journal).
* `erase_all()` requires `factory_reset confirm` + preserves fault log.

**Gates:** corrupt-NVS injection boots degraded with defaults + `NVS_CORRUPT` fault, no `ESP_ERROR_CHECK` abort.

### 6.9 Display and GUI isolation

**Deliver:**

* Single owner: only GUI task calls `esp_lcd_*` + `lv_timer_handler()`. Others send `UiCommand` queue (already partially via `CommandOverlay`, formalize).
* Frame budget: 500ms dashboard cadence kept, `handle_timer()` 200ms bound kept, DMA timeout 500ms → `DMA_TOUT` fault → auto re-init (recovery, not user-autonomous content).
* Panel contract: `send_module_init` table versioned per `TFT VER1.0`, `DISPON` after first GRAM fill stays, `BGR` order locked by test.

**Gates:** lock-timeout injection skips frame without corrupt, DMA-fail injection recovers in <2s.

### 6.10 Radio isolation

**Deliver:**

* Radio sandbox: Wi-Fi/NimBLE/LwIP live on Core0 behind `connectivity/*` facade. App never includes `esp_wifi.h`/`nimble/*` directly (extend header barrier).
* `MK_ISOLATE_RADIO=1` build still links IDF radio but core headers clean. `=0` display-only build links no `libfreertos.a` radio (documented, CI-checked via `nm`).

### 6.11 OTA and lifecycle

**Deliver:**

* A/B partitions (`partitions_8mb_ota.csv` already), `verify → mark_valid` after 5min `device_score>80`, else auto `rollback()`. Anti-rollback `APP_VERSION` monotonic gate.
* Staged download to PSRAM/SPIFFS with SHA256 + ECDSA verify before `esp_ota_begin()`. Progress events, not poll. `cert_bundle` default, `https://` enforced (keep `ota_service.cpp:28`).
* `release/` SBOM + `BUILD_TIMESTAMP + GIT_COMMIT` already in `app_main.cpp:107` — add `mk_version + health_policy_version`.

### 6.12 Security baseline

Secure Boot V2 + FlashEnc (reserve eFuse plan), NVS encryption for `wifi_pass`, BLE LE Secure Connections, TLS via bundle (fix `ota_service.cpp:51`), `wifi_ap` password rotated from `nexos1234` default, no secrets in logs.

### 6.13 Observability and manufacturing

`trace/` ring (stall, heap, event_drop, dma), `diag [boot|tasks|heap|evq|fault]`, `self-test` extended to MPU/stack/CRC, manufacturing mode (`hold BOOT 3s` → color bars + `burnin 1h`).

---

## 7. Management: scores, commands, dashboard

| Score | Source | Healthy | Degraded | Critical |
|---|---|---|---|---|
| Device | supervisor | 90-100 | 60-89 badge amber | <60 safe-mode |
| Memory | `heap_caps` tiers + frag index | frag<60%, free>40KB | frag 60-80% | frag>80% or free<24KB |
| Crash | fault record + reboot count | 0 faults/24h | 1 recovered | ≥3/10min or unrecovered panic |
| Net | Wi-Fi/BLE state + RSSI + SNTP age | connected+synced | AP-only or unsynced>1h | radio down |

CLI (both `src/main.cpp` Arduino and `command_service.cpp` IDF get same verbs):

```
health          -> device/mem/crash/net scores + fault_code
fault show      -> last 3 persisted faults
fault clear     -> clear acked (never clears unrecovered)
diag tasks|heap|evq|trace
mem pools|frag|leaks
coredump read|clear
safeboot        -> force safe-mode next boot
```

GC9A01 badges: green ring `SYSTEM OK`, amber `DEGRADED xx`, red `SAFE MODE`, always with `FW + uptime + heap` footer (keep `gui_dashboard.cpp` contract).

---

## 8. Roadmap and exit criteria

### Phase V2.0 — Native base (3-4 wk)

1. Port freeze + isolation CI (`check_port_isolation.py`, `nm` radio check).
2. Real scheduler + PI mutex + queue/event/timer wheel.
3. Nesting spinlock fix + MPU/stack guards.
4. Health record + supervisor skeleton + degraded UI mode.
5. Fault record + safe-mode boot.

Exit: all P0 gates in §6 pass, `esp32s3_arduino` + `esp32s3_idf` build, 24h soak, fault matrix green.

### Phase V2.1 — Prod-harden (3 wk)

Tickless + DVFS, TLSF + frag policy, OTA verify/rollback, NVS CRC/migrate, trace ring, HIL rig (logic-analyzer SPI + current probe), 72h soak, SBOM + signed release (`tools/generate_release_binaries.py` extended).

Exit: benchmark table published, MTBF >500h lab, zero `ESP_ERROR_CHECK` abort paths in boot.

### Phase V2.2 — Research spikes (time-boxed, not blocking)

`mk_context.S` preemptive spill, Rust shim for `event_bus`, verified queue proof sketch, ThreadX-style preemption-threshold experiment.

---

## 9. Benchmark quality gates (publish per release)

| Metric | Method | V2.0 gate |
|---|---|---|
| Ctx switch p99 | GPIO toggle + LA | <15µs |
| Timer jitter p99 | 1kHz periodic | <150µs |
| Mutex hold GUI | trace | <2ms p99 |
| LVGL frame skip | fault inject lock 300ms | skip, no corrupt |
| Flash / RAM core | `pio check` | core <12KB / <8KB excl. stacks |
| Soak | 72h dashboard + CLI fuzz | 0 panic, heap delta <4KB |
| Fault matrix | 12 injections | 12 correct mode + persisted cause |
| OTA | bad image + power-cut | rollback + safe boot |
| Build | `arduino + idf + simulator + tests` | green + isolation check |

---

## 10. Risks

* Xtensa windowed ABI preemption is the hardest item — contained behind `mk_port.h`, V2.0 does not require it.
* ESP-IDF radio mandates FreeRTOS linkage for full builds — mitigated by header quarantine + `nm` gate, not false “FreeRTOS-free binary” claims.
* PSRAM hides SRAM bugs — mitigated by tiered accounting + `MALLOC_CAP_INTERNAL` gates everywhere (keep `health_monitor.cpp:23` rule).

---

## 11. Backlog pointer (deferred, not designed here)

Autonomous user features (auto-provisioning UX, ambient brightness content, predictive content, fleet behaviors) remain in backlog. V2 provides the hooks they will use (`health_snapshot`, `UiCommand` queue, `OTA staged`, power domains) without implementing the behaviors.

---

## 12. File map for implementers

```
components/microkernel/
  include/mk_config.h      -> Kconfig-ify, remove single_core dup, freeze port ABI
  port/mk_port.h           -> frozen V2.0, add suspend/resume/watermark already done, add stats
  port/native/*            -> RSIL nesting, GPTimer tick, (V2.2) mk_context.S
  core/mk_scheduler.c      -> real bitmap scheduler (replace diagnostics-only)
  core/mk_task.c           -> ZOMBIE reap, MPU guard, quantum
  ipc/mk_mutex.c           -> PI + ceiling + deadlock debug
  ipc/mk_queue.c/event.c   -> atomic counts, waiter lists, no polls
  time/mk_timer.c          -> wheel + drift stat
  memory/                  -> pools + TLSF + frag index + leak ledger
  core/mk_health.c (new)   -> scores + supervisor policy
  core/mk_crash.c (new)    -> fault record + dump + safeboot
  core/mk_power.c (new)    -> tickless + DVFS + domains
  core/mk_trace.c (new)    -> ring + diag export
```

---

## 13. QA plan (benchmark-grade)

* Contract tests: `test/kernel/` — create/delete churn, timeout, PI, queue burst, timer drift, nesting, MPU guard.
* HIL: DevKitC-1 + GC9A01 + LA + current probe; `tools/hil_run.py` runs frame-budget + brownout + DMA-fail rigs.
* Fault injection: `tools/fault_inject.py` — stall, heap pressure, event flood, NVS corrupt, OTA bad image, SPI noise.
* Release: `release/` signed binaries + SBOM + `FLASH_INSTRUCTIONS.md` + `health_policy_version`.
* No release without §9 table attached to CHANGELOG.

---

## 14. Phase-1 implementation record 2026-09-04 (prod-grade hybrid)

Honest model shipped: **Nexos-RT API owned, execution via FreeRTOS port**. No false standalone claim.

* `port/mk_port.h:36` + `port/native/mk_port_native.c:55` + `port/freertos/mk_port_freertos.c:39` — new `mk_port_task_set_priority()`. `ipc/mk_mutex.c:71` PI now bridges to `vTaskPrioritySet` on boost/unboost, shadow queues kept for accounting.
* `core/mk_scheduler.c:222` + `include/mk_scheduler.h:24` — 1kHz `esp_timer` accounting tick started from `core/mk_kernel.c:100` `mk_start()`. Quantum/sleep/jitter live; preemption remains FreeRTOS.
* `core/mk_health.c:35` — snapshot/mode locked via `mk_port_enter/exit_critical`, sub-score recovery added.
* `core/mk_crash.c:21` — RTC counter deterministic init (`0` on first power-on), retained-fault log clarified, explicit `fault clear` contract.
* `src/main.cpp:25,767` — WDT/brownout left enabled, `init_ble` Core0 WDT disable removed, boot feeds kept.
* `platformio.ini:20` — Arduino restored to `partitions_8mb_arduino.csv` (`0x10000`); IDF keeps `ota.csv` (`0x20000`). `tools/generate_release_binaries.py:16` + `release/flash_device.ps1:158` fixed to `0x10000` for Arduino.
* Creds: `WIFI_AP_PASSWORD_DEMO_DEFAULT`, PSK masked as `[hidden]` in `wifi_service.cpp:110`, `src/main.cpp:213,556,649`, `command_registry.cpp:152,163`. BLE hello no longer broadcasts PSK.
* Display: `src/main.cpp:69` default `4MHz` for dupont integrity, factory `8MHz` documented.
* Repo: `release/*.bin` untracked via `.gitignore:26` + `git rm --cached`.
* Validation: `test_nexos_base_os.py:13` + `run_tests.py:32` = **45/45 OK**. `pio run -e esp32s3_arduino` **SUCCESS** RAM `20.9%` Flash `43.1%`. Release merge at `0x10000` verified `1421456B`.

HIL still required on hardware: splash → dashboard → BLE 2.5s → AP 6s, `wifi_ap`, `ble_status`, `health`, 1h soak, brownout/RF coex check.

*End of V2 upgrade plan Phase-1 record.*

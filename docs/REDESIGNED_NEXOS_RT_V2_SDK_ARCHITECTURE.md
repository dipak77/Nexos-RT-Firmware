# Redesigned Nexos-RT V2 SDK Architecture — V2.0 Lite Implementation Plan

**Document:** Phase-wise implementation plan, `.md` only (no code changed by this document)
**Scope:** V2.0 Lite on ESP32-S3 only. V2.5 Pro (ARM64 MMU/TTBR/ASID) and V3.0 (ML scheduler, cert) are explicitly deferred — one short handoff section at the end, no design detail here.
**Baseline:** Nexos-RT v1.2.0 (`components/microkernel/include/mk_config.h:1`, `mk_types.h:1`, `mk_scheduler.h:1`) — `mk_*` wrapper over FreeRTOS, 3 app tasks on Core1, radio on Core0.
**Patent input:** 17-claim gap analysis (enclaves, deadline/budget, SVC gate, per-enclave WDT, TTBR/ASID, fault ring). This plan implements the **Lite subset** honestly buildable on ESP32-S3.

---

## 0. V2.0 Lite in one page

Same `mk_*` API apps use today, now backed by enclave contracts enforced in software on ESP32-S3:

* 4 enclaves max (GUI / SYSTEM / CLI / AUDIO+INFER) via 8 MPU regions.
* `mk_enclave_desc_t` per task: base/limit, stack, MPU-region-id in `asid` field, deadline/budget/priority/type, watchdog id, syscall mask, state.
* Software SVC gate (no EL0/EL1 hardware on Xtensa): mask check + pointer-in-enclave check + entry-point allowlist, <50µs budget, no heap in gate path.
* FPPS + deadline ordering + budget enforcement + RT-preempts-INFER deferral. Deterministic fallback only (no ML in V2).
* Per-enclave watchdog + `reset_enclave()` sanitize + structured ring fault log.
* Portable HAL: `mk_chip_port.h` + `esp32s3` MPU backend. Pro MMU backend plugs in later without touching core call sites.
* Minimal POSIX shim (pthreads, mqueue, timers) so Arduino code migrates ~90%.
* Ships in ~2 weeks on the GC9A01 board you have.

---

## 1. Goals and non-goals (V2.0 Lite)

### Goals

1. Keep `mk_task_create`, `mk_mutex_lock`, `mk_watchdog_*`, `mk_*` IPC/timer/pool ABI source-compatible. App code unchanged except adopting enclave descriptors and capability calls.
2. Prove isolation on S3: GUI fault cannot corrupt SYSTEM SRAM; stack overflow traps to enclave FAILED, not whole-board reboot.
3. Prove timing contracts: deadline-ordered FPPS, budget overrun → per-enclave WDT trap, INFER deferred when RT deadline < threshold.
4. Prove privilege hygiene: no direct HW access from enclaves; all driver access via SVC-validated capability (`GFX|NET|GPIO`).
5. Ship SDK pack firms can adopt: headers, Kconfig/port docs, CLI parity (`kernel_info`, `wifi_ap`, `ble_status`), QEMU/host unit proof + on-device proof, fault-injection evidence.

### Non-goals (deferred to V2.5/V3, not designed here)

* ARM64/RISC-V MMU, TTBR0_EL1, ASID-tagged TLB, EL0/EL1 hardware separation, page-table revoke.
* ML predictor scheduler (Claim 6 full), formal verification, ISO26262 ASIL-B / IEC61508 packs.
* New user features (provisioning UX, Matter, fleet). Autonomous behaviors stay in backlog.

---

## 2. Honest baseline (V1.2)

What is solid — preserve:

* `mk.h`-only app barrier, slot lifecycle `FREE→RESERVED→LIVE→DELETING`, launch gate `mk_start()`, `DisplayGuard` RAII, 1ms tick config, internal-SRAM heap accounting, task states incl. `ZOMBIE`.
* Scheduler bookkeeping exists (`mk_scheduler.h:7` bitmap + quantum + sleep list + 1kHz `esp_timer` accounting driver) but **execution is still FreeRTOS-driven** (hybrid). V2.0 Lite keeps this execution model and makes the **contracts** real; it does not claim a from-scratch context switcher on Xtensa.

What V2.0 Lite must add (all buildable on S3):

Enclave manager + descriptor, MPU-backed isolation, software SVC mask gate, FPPS deadline/budget enforcement, per-enclave WDT + sanitize + ring log, pthread shim. ML scheduler stays out.

---

## 3. Patent-claim coverage — Lite subset only

| Patent item | Lite (S3) mechanism | Honest S3 adaptation |
|---|---|---|
| Claims 1,5,10,11 enclave assignment + isolation | `mk_enclave_desc_t` lifecycle `FREE→RESERVED→LIVE→FAILED→(reset)→FREE`; MPU region per enclave | No TTBR/page tables; `asid` field carries **MPU region id** (forward-compat for ARM64 ASID) |
| Claim 2 FPPS + deadline/budget, RT vs INFER | Deadline-ordered FPPS, budget via 1kHz tick, overrun → WDT trap; INFER suppressed when RT deadline < threshold | Software enforcement; <50µs IRQ target is a budget, measured on device, not HW-guaranteed |
| Claim 3 SVC gate + EL0/EL1 | Software SVC dispatcher: `syscall_mask` check, arg pointer must lie in enclave `base/limit`, entry-point allowlist, no heap in path | No Xtensa EL0/EL1 or trap HW; enforcement is a validated function gate, documented as such |
| Claim 4 minimal POSIX | `pthread_create/join`, `mq_*`, timers mapped to `mk_*` | Subset only; no glibc |
| Claim 7 per-enclave WDT | Per-enclave deadline+budget monitor, `reset_enclave()` zeroes stack+desc, IPC halted | Software tick-driven; HW WDT stays as last resort |
| Claim 8 ring audit | Fixed-size structured ring `ID\|type\|code\|ts`, no printf in hot path | SRAM ring + CLI dump; core-dump partition optional |
| Claim 6 ML predictor | **Out for V2** — deterministic FPPS fallback ships; ML hook struct reserved | V3 topic only |

---

## 4. Target architecture (5 layers, one API)

```
Application            mk_gfx, mk_audio, mk_net only (no direct driver calls)
POSIX Shim + mk_* SDK  pthreads, mqueue, timers, CLI  (thin mapping, V2 subset)
Microkernel (priv)     Enclave Mgr | FPPS Scheduler | SVC Gate | Fault Mgr | Zero-copy IPC
HAL (portable)         mk_chip_port.h :: esp32s3 backend (MPU 8 regions, timer, WDT, ring)
Hardware               ESP32-S3 Lite (Xtensa LX7, no MMU, no EL0/EL1) — S3 limits documented
```

Rules:

1. Core never includes `freertos/*.h`, `lwip/*`, `nimble/*`. Only `port/*` and HAL backends may. Enforced by header-barrier test.
2. No heap in SVC gate, scheduler pick, or fault-trap paths. Static pools only.
3. Every cross-enclave call carries capability token + handle; kernel validates borders before copy.
4. `mk_port.h` ABI frozen for V2.0 Lite; S3-Pro differences live behind HAL, never in core call sites.

File map (new vs touch):

```
NEW  components/microkernel/include/mk_enclave.h      descriptor + lifecycle + caps
NEW  components/microkernel/core/mk_enclave.c         manager (create/revoke/reset/sanitize)
NEW  components/microkernel/core/mk_svc.c             software SVC dispatcher + allowlist
NEW  components/microkernel/core/mk_fault.c           ring log + trap entry (Claim 7/8 Lite)
NEW  components/microkernel/port/esp32s3/mk_mpu_s3.c  MPU 8-region backend + stack guards
NEW  components/posix_shim/*                          pthread/mqueue/timer subset
TOUCH components/microkernel/include/mk_types.h       add enclave fields (keep mk_task_t layout compat)
TOUCH components/microkernel/core/mk_scheduler.c      deadline key + budget check + INFER defer
TOUCH components/microkernel/ipc/*                    token + border checks on copy paths
TOUCH components/microkernel/core/mk_diagnostics.c    per-enclave WDT view (keep global feed compat)
TOUCH tools/check_port_isolation.py                   extend barrier to enclave/SVC paths
```

---

## 5. Enclave descriptor — core data structure (Lite)

One descriptor per task, owned by the kernel. Enclaves never modify it; only SVC/manager transitions state.

```c
typedef struct {
  /* memory isolation (Lite: MPU; Pro later: MMU page table) */
  uint32_t base, limit;
  uint32_t stack_base, stack_size;
  uint32_t asid;          /* Lite: MPU region id. Pro: ARM64 ASID. Same field, forward-compat. */
  /* timing contract — Claim 2 */
  uint32_t deadline_us;
  uint32_t budget_us;
  uint8_t  priority;      /* FPPS */
  uint8_t  type;          /* MK_ENCLAVE_RT / MK_ENCLAVE_INFER */
  /* fault + privilege */
  uint8_t  watchdog_id;   /* per-enclave WDT */
  uint32_t syscall_mask;  /* e.g. CAP_GFX|CAP_NET|CAP_GPIO — never RW mem */
  uint8_t  state;         /* FREE -> RESERVED -> LIVE -> FAILED */
} mk_enclave_desc_t;
```

Lifecycle: `FREE → RESERVED (validate metadata) → LIVE (MPU programmed) → FAILED (trap) → reset_enclave() zeros stack+desc → FREE`. GUI crash marks GUI FAILED, revokes its MPU window, halts its IPC stream; SYSTEM keeps running.

S3 capacity: 4 enclaves max — GUI, SYSTEM, CLI, AUDIO+INFER. MPU region budget documented in HAL (8 regions total, reserve 2 for kernel flash/SRAM windows).

---

## 6. Scheduling — FPPS + deadline + budget (Lite, deterministic, no ML)

* Ordering key: `(priority, deadline_us)`. FPPS first, earliest deadline wins ties. No ML predictor in V2 hot path.
* Budget: per-enclave `budget_us` decremented on 1kHz tick while RUNNING. Overrun → WDT trap → FAILED (Claim 2/7). WCET table per enclave published in `docs/` from measurement, not theory.
* RT vs INFER: `MK_ENCLAVE_RT` preempts; `MK_ENCLAVE_INFER` (audio/infer) is non-blocking, yields when any RT deadline < threshold, resumes on slack. Suppressed, never killed, unless budget overrun.
* V1.2compat: existing `MK_PRIO_*` map becomes the FPPS base; `deadline_us/budget_us` default to `0` = legacy prio-only behavior until callers adopt descriptors. No flag-day.

---

## 7. Privilege — software SVC gate (S3-honest)

Xtensa has no EL0/EL1 or SVC trap HW, so V2.0 Lite implements the gate as a validated dispatcher (same call sites Pro will trap on):

```
Enclave: mk_gfx_draw() -> mk_svc(#GFX_DRAW, args)
  -> mask check: desc->syscall_mask & CAP_GFX else trap
  -> arg sanitize: pointers must lie in [base, limit); sizes bounded
  -> entry-point allowlist only (no ROP gadgets)
  -> copy_to_kernel(); quarantined HAL driver; copy back result
```

Constraints: <50µs budget measured, no `malloc` in path, all failures trap to Fault Mgr with `ID|type|code|ts`. V1.2 direct pattern (`mk_mutex_lock` + `spi_write` from app) becomes a porting error caught by barrier test + code review checklist.

---

## 8. Memory isolation — S3 MPU Lite

* 8 MPU regions: 2 kernel (flash RX, SRAM RW), up to 4 enclave windows (code+data+stack per enclave where region count allows; else data+stack window + shared RX text), 1 peripheral window (SVC-only), 1 guard.
* Stack sanitized (`0x00`, not just watermark) on `reset_enclave()`.
* No dynamic alloc in gate/scheduler/trap. Enclave heaps are static pools carved at CREATE; OOM returns `MK_ERR_NO_MEMORY`, never steals from another enclave.
* Zero-copy IPC: sender buffer must be inside sender enclave; kernel maps/copies once into receiver window after border check; no shared RW.

---

## 9. Fault handling — per-enclave WDT + ring log (Lite)

* Per-enclave deadline+budget monitor on 1kHz tick (extends global `mk_watchdog_feed_self` which stays for compat).
* Trap sequence: halt task → revoke MPU window → halt its IPC stream → mark FAILED → `reset_enclave()` sanitize → ring log `ID|type|code|ts` → optional policy restart (GUI restarts; SYSTEM never auto-restarts without operator Ack in Lite).
* Ring: fixed 128-entry SRAM, overwrite-oldest, CLI `fault show/clear`, persisted last-N across reboot where RTC/NVS allows. No printf in trap path.

---

## 10. POSIX shim — minimal V2 subset

Map only what Arduino code needs to migrate ~90%: `pthread_create/join`, `pthread_mutex_*`, `mq_open/send/receive`, `timer_create/settime`, `sleep/msleep`. Everything else returns `ENOSYS` with a porting note. Shim sits above `mk_*`, never beside it. Full pthreads is V2.5.

---

## 11. HAL portability contract

`mk_chip_port.h` exposes: `mpu_program(window)`, `mpu_revoke(id)`, `timer_1khz_start/stop`, `wdt_arm/pet`, `ring_log_push`, `core_id()`, `irq_save/restore`. S3 backend implements with MPU+`esp_timer`. ARM64 backend later implements TTBR/ASID without core changes. HAL is 100% quarantined: drivers never called directly from enclaves.

---

## 12. SDK pack — what firms get in V2.0 Lite

Headers (`mk_enclave.h`, `mk_*`), Kconfig/port doc for S3 MPU windows, capability catalog (`CAP_GFX|CAP_NET|CAP_GPIO|CAP_I2C`), WCET/budget worksheet, SystemView-style trace hooks, unified CLI (`kernel_info`, `enclave show`, `fault show`, `wdt show`), OTA-with-rollback + NVS reuse (already in repo), QEMU/host unit proof + on-device proof on GC9A01 board, fault-injection evidence set.

---

## 13. Implementation phases — V2.0 Lite only (~2 weeks)

### Phase 0 — Freeze baseline (0.5 day). Exit: green main + listed proofs rerun.

* Tag V1.2 demo image; record `RAM/Flash`, boot log, `tasks/health` golden output.
* Lock `mk_port.h` ABI for Lite; add `tools/check_port_isolation.py` to CI (fail on `freertos/*` outside `port/*`, HAL).
* Tests: existing `test/run_tests.py` + `test/test_nexos_base_os.py` green.

### Phase 1 — Enclave manager + S3 MPU HAL (Week 1). Exit: 4 enclaves isolated on device.

* Tasks: `mk_enclave.h/.c` lifecycle; `mk_mpu_s3.c` program/revoke/guard; extend `mk_types.h` without breaking `mk_task_t` ABI (new fields appended or parallel desc table); MPU window map doc.
* Proof: GUI OOB write traps to GUI FAILED, SYSTEM alive; `enclave show` lists 4 LIVE with base/limit/region; stack-guard trip test passes.
* Files: NEW enclave/mpu; TOUCH types/config/Kconfig; TEST host MPU-window unit + on-device OOB drill.

### Phase 2 — Scheduler budget + SVC gate (Week 2, first half). Exit: deadline/budget + mask enforced.

* Tasks: deadline key in pick, budget decrement + overrun trap, INFER defer threshold; `mk_svc.c` mask + sanitize + allowlist; convert `mk_gfx` display path first (highest risk), keep other drivers direct with deprecation warnings.
* Proof: RT&apos;02 Bowl preempts INFER under load; budget-overrun drill traps in <2 ticks; forged-capability call trapped; gate p99 <50µs measured.
* Files: TOUCH scheduler/ipc/diagnostics; NEW svc; TEST budget-overrun + mask-forge + latency microbench.

### Phase 3 — WDT + ring + SDK pack + QA (Week 2, second half). Exit: shippable Lite SDK.

* Tasks: per-enclave WDT + `reset_enclave()` sanitize + 128-entry ring + `fault show/clear`; POSIX subset; CLI parity; OTA/NVS reuse docs; fault-injection matrix (stall, heap, DMA fail, NVS corrupt, SPI noise); benchmark table (latency, heap isolation, gate time).
* Proof: kill GUI → auto FAILED + sanitize + ring entry + SYSTEM uninterrupted; 24h soak; SDK builds Arduino blink + display demo unchanged except enclave descriptors.
* Files: NEW fault/posix/docs; TOUCH diagnostics/CLI/tools; TEST full matrix green.

Each phase ends with: build both envs, unit green, on-device proof log attached, doc updated. No phase starts with red tests.

---

## 14. Verification and benchmarks (Lite)

* Host unit: descriptor lifecycle, bitmap pick, PI-equivalent boost (existing tests extended), mask matrix, ring overwrite.
* On-device: OOB trap, stack-guard trip, budget overrun <2 ticks, forged SVC trapped, gate p99 <50µs, GUI-kill/SYSTEM-alive, 24h soak heap delta <4KB.
* Benchmarks published: IRQ-to-gate time, pick time, gate time, heap per enclave, fault-matrix pass/fail. S3 numbers labeled S3; no ARM64 claims in Lite.

---

## 15. Risks and open decisions

* S3 MPU region pressure (8 total) — mitigation: share RX text, window data+stack, document map; 4-enclave cap enforced at CREATE.
* No HW privilege — mitigation: honest software gate + barrier test + review checklist; document as Lite limitation.
* Budget accounting via 1kHz tick (not cycle-accurate) — mitigation: conservative WCET margins, overrun drill evidence.
* POSIX subset friction — mitigation: `ENOSYS` + porting notes, Arduino demo as reference migration.
* Decision log required: region map, default deadline/budget per enclave, INFER threshold, restart policy (GUI yes / SYSTEM no), ring size.

---

## 16. V3 handoff (deferred, short)

V2.5 Pro (ARM64 TTBR/ASID/EL0/EL1, full revoke, POSIX fuller, QEMU) and V3.0 (ML scheduler + deterministic fallback per Claim 6, MISRA/ASIL-B packs, formal verification) reuse this Lite core: same `mk_enclave_desc_t` (`asid` becomes real ASID), same lifecycle, same SVC call sites (trap HW replaces function gate). No V3 design in this document.

---

## Appendix A — Claim → Lite test traceability

| Claim | Lite proof |
|---|---|
| 1,5,10,11 enclaves | OOB drill + `enclave show` + region map |
| 2 deadline/budget, RT/INFER | Overrun trap drill + defer drill + latency table |
| 3 SVC/mask | Forge drill + sanitizer drill + allowlist test |
| 7 per-enclave WDT | Kill-GUI/SYSTEM-alive + sanitize check |
| 8 ring log | Overflow + `fault show` + reboot-retain check |

## Appendix B — CLI additions (Lite)

`enclave show`, `enclave reset <id>`, `fault show`, `fault clear`, `wdt show`, `svc stats`, existing `kernel_info/status/tasks/health/version` unchanged.

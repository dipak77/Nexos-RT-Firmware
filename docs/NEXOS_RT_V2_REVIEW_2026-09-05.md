# Nexos RT V2 architecture and UI review — 5 September 2026

Review result: changes are not ready to sign off as the V2 architecture described in the plan. The Arduino build passes, but the IDF product build fails. Several lifecycle and scheduling bugs remain, and the UI has reproducible layout and data-presentation defects.

Scope: V2 changes from `7029085` through `bb91222`, including the working changes present at the beginning of the review that became commit `bb91222` during the review. Existing Arduino and LVGL UI were also inspected at the user's request; those UI findings are not attributed to the V2 commits. Firmware source was not changed by this review.

Validation performed:

- `python test/run_tests.py`: all 55 tests passed.
- `python tools/check_port_isolation.py`: passed.
- PlatformIO `esp32s3_arduino`: incremental build passed; static RAM 73,308 bytes, flash 1,365,961 bytes. This does not measure runtime free memory or prove fault recovery.
- PlatformIO `esp32s3_idf`: failed during CMake component resolution, before application compilation.
- Inspected the installed SDK headers, actual Adafruit font tables, both UI implementations, and `dispaly-screenshot.jpeg`.
- Checked circular display geometry and the bottom-pill glyph pixels against the bundled bitmap font. Pill outlines exceed the circle; the tested Arduino Wi-Fi/BLE label glyphs themselves fit.
- No firmware was flashed. No hardware fault drill, current dashboard capture, or native LVGL rendering was performed. The saved photo shows display corruption, not a readable current dashboard. The CMake simulator is a placeholder, and the Python/web simulator does not execute the firmware renderer.

## Architecture defects — fix before V2 acceptance

### A1 · P1 · IDF product build is broken

[Microkernel CMake dependency](C:/Users/haran/source/repos/smart_device_firmware/components/microkernel/CMakeLists.txt:32)

The build fails with `Failed to resolve component 'esp_ipc' required by component 'microkernel': unknown name`. The IDF environment resolves Espressif platform 7.0.1 / IDF 6.0.1; its `esp_ipc.h` belongs to `esp_system`, which is already required. Arduino builds do not use this component dependency graph, so Arduino success misses the defect.

Fix the dependency for the supported SDK, pin the IDF platform version, and build both environments in CI. After resolving CMake, verify the manually redeclared FreeRTOS functions against that SDK: `pxTaskGetStackStart(void*)` differs from IDF 6's `TaskHandle_t` declaration, and runtime counter types are configurable.

### A2 · P1 · Priority inheritance corrupts ready queues

[Mutex boost](C:/Users/haran/source/repos/smart_device_firmware/components/microkernel/ipc/mk_mutex.c:150), [ready-list removal](C:/Users/haran/source/repos/smart_device_firmware/components/microkernel/core/mk_scheduler.c:101)

The code assigns the new priority before `mk_scheduler_remove_ready()`. Removal indexes the queue using that new priority, although the task is still linked in its old queue. With a lone low-priority owner and a high-priority waiter, this can erase the high-priority queue head while leaving the owner in the low-priority queue. Adding the owner again then gives inconsistent heads, links, and bitmap state. The reboost and unboost paths repeat the same ordering.

Remove from the old queue first, change priority, then insert into the new queue under one lock. Add an actual C test with multiple priorities and head/middle/tail owners. Separately, effective priority must account for waiters on all held mutexes: unlocking an unrelated mutex currently restores base priority even if another mutex still has a higher-priority waiter.

### A3 · P1 · Completed/self-faulted enclaves retain freed task and stack references

[Self-trap](C:/Users/haran/source/repos/smart_device_firmware/components/microkernel/core/mk_enclave.c:194), [reclaim](C:/Users/haran/source/repos/smart_device_firmware/components/microkernel/core/mk_enclave.c:264), [task deletion](C:/Users/haran/source/repos/smart_device_firmware/components/microkernel/core/mk_task.c:127)

Self-trap deletes the task without clearing the enclave's task handle or stack window. Normal entry return also deletes the task without notifying the enclave. The port idle task can free the underlying task/stack, but a later enclave reset still calls suspend on the retained backend handle and clears the retained stack address. `mk_task_suspend()` accepts a slot with a port handle without requiring it to be LIVE. This can access freed memory or erase memory reused by another allocation.

Concrete trigger: run `enclave_drill` with runtime statistics unavailable. The burner returns after two seconds; the command resets its descriptor after approximately three seconds. The retained references are then unsafe. Do not execute this drill on a valued running device until the path is fixed.

Add a unified task-exit notification and deferred reaper. Invalidate descriptor ownership before backend deletion, use generation-checked handles, and sanitize only memory whose ownership is still established. Check suspend/delete results before proceeding.

### A4 · P1 · Task exits exhaust the task pool; self-reclaim strands enclave slots

[Task tombstone](C:/Users/haran/source/repos/smart_device_firmware/components/microkernel/core/mk_task.c:130), [self-reclaim](C:/Users/haran/source/repos/smart_device_firmware/components/microkernel/core/mk_enclave.c:253)

Self-deleting task slots remain `SLOT_DELETING` forever; there is no reaper that returns them to `SLOT_FREE`. This inherited limitation becomes significant with the new POSIX threads and sacrificial enclaves. Repeated short-lived threads eventually exhaust the 16 slots even though the reported live task count decreases. A self-call to enclave reclaim likewise deletes the current task before the descriptor can reach FREE, leaving it RECLAIMING; later reset rejects that state.

Complete both lifecycles from a surviving manager after the backend confirms task exit. Validate at least 100 create/return/join cycles, plus self-trap and self-reclaim, with stable available slots and memory.

### A5 · P1 · An unregistered caller can inherit another enclave's identity

[Current task fallback](C:/Users/haran/source/repos/smart_device_firmware/components/microkernel/core/mk_scheduler.c:297)

When `mk_task_self()` finds no registered task, the function returns the global `s_current_task` cache. Vendor callbacks and timer tasks can therefore appear to be the last Nexos task that set that cache. SVC checks can borrow that task's capabilities, and a failed check can trap the unrelated enclave. Mutex ownership attribution can also attach a foreign task's mutex to the wrong enclave.

In this FreeRTOS execution model, return NULL for unregistered callers. Give trusted backend services an explicit identity/API instead of silently borrowing an application identity. Test a foreign callback after a privileged enclave has run.

### A6 · P1 · Dynamically started tasks run before enclave binding

[Enclave start](C:/Users/haran/source/repos/smart_device_firmware/components/microkernel/core/mk_enclave.c:107), [task entry wrapper](C:/Users/haran/source/repos/smart_device_firmware/components/microkernel/core/mk_task.c:26)

The task is created before its enclave, budget, deadline, and stack window are attached. The global launch gate only protects startup before `mk_start()`; once open, a newly created higher-priority task can execute immediately, including before its task slot is fully published. Its first SVC call can be unbound or misattributed, and a quick return can attempt deletion while the slot is RESERVED.

Use a per-task creation latch: fully publish the task and enclave, validate metadata and stack ownership, then release the entry function. Also insert/reinsert the task into deadline order after setting its deadline; the current initial insertion sees deadline zero.

### A7 · P1 · Watchpoints are managed on the caller's CPU, not the enclave's CPU

[Watchpoint arming](C:/Users/haran/source/repos/smart_device_firmware/components/microkernel/core/mk_enclave.c:139), [trap teardown](C:/Users/haran/source/repos/smart_device_firmware/components/microkernel/core/mk_enclave.c:217), [watchpoint driver](C:/Users/haran/source/repos/smart_device_firmware/components/microkernel/arch/esp32s3/mk_watchpoint.c:28)

Installed `esp_cpu.h` explicitly says watchpoints operate on the current CPU. Enclaves are pinned to core 1, but management can run on another core, including the ESP timer task. Cross-core disarm therefore need not clear the original guard. Teardown subsequently writes zero into that guarded stack. A stale guard can panic on cleanup or later memory reuse. Enclave IDs 0/1 are also treated as permanent hardware slots, without coordinating with FreeRTOS stack guarding; the Arduino SDK enables its own end-of-stack watchpoint support.

Manage guards through a core-aware port operation and coordinate hardware slot ownership with the vendor runtime. Do not zero the guard region until the owning CPU confirms disarm. These debug watchpoints cause CPU panic; they do not implement the advertised isolated enclave overflow recovery.

### A8 · P1 · SVC memory checks omit writable outputs

[SVC buffer checks](C:/Users/haran/source/repos/smart_device_firmware/components/microkernel/core/mk_svc.c:109), [result write](C:/Users/haran/source/repos/smart_device_firmware/components/microkernel/core/mk_svc.c:156)

Only GFX_FLUSH and NET_SEND payloads are checked. NET_RECV and IPC receive buffers are not validated, `args` is dereferenced without checking its envelope, and `out_result` is written unconditionally when non-null. Even GET_TIME_US can write to an address outside the caller's declared window through `out_result`. This defeats the pointer-validation contract regardless of whether hardware memory isolation is present.

Define argument schemas per SVC and validate all read/write spans, including argument/result structures, before dereferencing or dispatching. Keep trusted kernel calls distinguishable from application calls. Test boundary, null/nonzero-length, overflow, and cross-enclave output cases in C.

### A9 · P2 · Budget accounting drops initial execution and races descriptor reuse

[Budget sampler](C:/Users/haran/source/repos/smart_device_firmware/components/microkernel/core/mk_enclave.c:318)

The first observed runtime counter becomes a baseline without charging any of the CPU time already consumed. A task can use its whole 8 ms budget and finish before a chargeable sample. Subsequent checks happen at 100 ms intervals, so this is not millisecond enforcement. Snapshot validation checks only LIVE state, not the captured task identity/generation; a reclaimed and reused slot can be charged or trapped using the prior task's sample. The LIVE-to-FAILED claim is also not atomic with the check: the trap happens after unlocking.

Establish the baseline at task creation, define counter units and wrap behavior, and validate an immutable task generation when charging and claiming the trap. Specify whether a budget is lifetime CPU consumption or a replenished per-job budget; currently there is no replenishment API. Report unavailable statistics explicitly rather than treating the drill's absence of a fault as proof that statistics were unavailable.

### A10 · P2 · IDF watchdog diagnostic queries the Arduino task name

[Changed diagnostic](C:/Users/haran/source/repos/smart_device_firmware/components/command/command_registry.cpp:362), [IDF task registration](C:/Users/haran/source/repos/smart_device_firmware/main/app_main.cpp:239)

The change replaces COMMAND with CLI, but this registry is used by the IDF product, which actually creates and registers `COMMAND`. `CLI` is the Arduino task name. Consequently `wdt_show` reports zero for the real command watchdog. Enumerate registrations or use the correct name for each firmware track.

## Missing architecture coverage and SDK improvements

- **Driver access does not traverse SVC.** There are no application call sites for `mk_svc_dispatch()` and no driver registrations outside its implementation. Arduino still calls TFT, Wi-Fi, and BLE directly. Capabilities are descriptor metadata until the service boundaries adopt them.
- **Memory isolation is absent.** No MPU backend or enforced ownership windows implement the plan's cross-enclave SRAM protection. Descriptors are publicly writable; applications can obtain and alter their own capability masks/windows. Document this as a trusted-code software contract until actual enforcement exists.
- **EDF/INFER scheduling is bookkeeping.** `mk_scheduler_pick_next()` has no execution call site; both ports create FreeRTOS tasks. Changing the shadow quantum does not make an imminent-deadline RT task preempt INFER. Do not advertise the shadow queue as measured deadline enforcement.
- **Boot budgets are disabled and IDF tasks are not enclave-bound.** Arduino boot enclaves use zero budget; IDF `spawn_nextos()` creates raw tasks. The product track therefore lacks the V2 binding exercised by the Arduino boot path.
- **Recovery is incomplete.** `watchdog_id` is metadata, not a registered enclave timeout-to-trap link. DMA abort only clears a flag; IPC peers are not woken with PEER_TERMINATED. Reset frees a slot but does not restart its service. Stack/data scrubbing and retained audit metadata need an explicit ownership policy.
- **POSIX is a subset with behavioral mismatches.** Mutex `trylock` does not report owner death like `lock`; static lazy mutex initialization races between two first users. Join has no self-join check, discards the start routine's result, and can attach to a reused raw slot. Message queues discard the original message length/priority and receive returns the fixed slot size. Document unsupported semantics or implement them with conformance tests before claiming migration compatibility.
- **C++ lifetimes need ownership rules.** Enclave's destructor skips LIVE and FAILED descriptors; `reclaim()` leaves the wrapper holding a reusable descriptor pointer. A later wrapper operation can target its replacement. `Thread::create()` leaks its heap wrapper if task creation fails; `Queue<T>` raw-copies arbitrary nontrivial C++ types. Use generation-aware owning handles, rollback on failure, and constrain queue payloads to trivially copyable types.
- **Fault evidence is duplicated.** SVC logs a fault, then `mk_enclave_trap()` logs it again with zero PC/SP/extra. One bad call consumes two ring entries and inflates totals. Pass one complete record through the trap path.
- **Tests overstate coverage.** String-presence tests, file-existence checks, and independently rewritten Python algorithms do not test the compiled C implementation. Add a host port for real C lifecycle/mutex/SVC tests and on-device scheduling/fault drills. Include both build environments and both renderers in validation.

## UI defects — existing Arduino and IDF interfaces

### U1 · P1 · LVGL updates continue after lock acquisition fails

[GUI thread](C:/Users/haran/source/repos/smart_device_firmware/main/app_main.cpp:369), [GUI state updates](C:/Users/haran/source/repos/smart_device_firmware/components/gui/gui.cpp:58)

`LvglLockGuard` has a finite timeout, but this call site never checks it before updating the dashboard. `Gui::update_state()` also ignores `runtime.lock()`'s return. Under contention, UI mutations can race LVGL processing and damage object state. Check the guard before every LVGL access and queue state updates to one GUI owner. A skipped frame must really skip drawing.

### U2 · P2 · Clock repeats seconds and permanently displays PM

[Clock update](C:/Users/haran/source/repos/smart_device_firmware/components/gui/gui_dashboard.cpp:306), [time formatter](C:/Users/haran/source/repos/smart_device_firmware/components/time_service/time_service.cpp:113)

In 24-hour mode the producer supplies `HH:MM:SS`; the renderer then appends a separate `:SS` label. In 12-hour mode the producer already supplies AM/PM, while a second label remains hardcoded to PM. A morning time can display contradictory AM and PM text, and the oversized row worsens clipping.

Store or format HH:MM, seconds, and period separately. Hide the period in 24-hour mode and update it from the actual time in 12-hour mode. Verify midnight, noon, 09:59:59, 23:59:59, and unsynchronized state.

### U3 · P2 · Divider crosses the date text

[Date placement](C:/Users/haran/source/repos/smart_device_firmware/components/gui/gui_dashboard.cpp:174), [divider placement](C:/Users/haran/source/repos/smart_device_firmware/components/gui/gui_dashboard.cpp:182)

The date starts at y=96 using the 12-pixel Montserrat font. The one-pixel divider is centered at y=102, inside that label's vertical band, and is created later, so it paints across the text. The time container also extends to y=108, competing for the same band.

Lay out clock, date, divider, and status chip in one vertical layout with explicit padding and gaps. Remove unnecessary default padding from the time container. Do not fix one overlap by moving a divider into the status chip.

### U4 · P2 · UI elements exceed the round panel's safe area

[Arduino bottom pills](C:/Users/haran/source/repos/smart_device_firmware/src/main.cpp:476), [LVGL top pills](C:/Users/haran/source/repos/smart_device_firmware/components/gui/gui_dashboard.cpp:77), [LVGL footer](C:/Users/haran/source/repos/smart_device_firmware/components/gui/gui_dashboard.cpp:273)

For the 240×240 circle centered at (120,120), the visible x interval at y=211 is approximately 41.8–198.2. Arduino pills occupy x=30–117 and 122–209 over y=192–211; their curved outer borders cross the circle and are cut off. Checked glyphs for WIFI OFF/ON and BLE OFF/ADV fit, so the verified Arduino defect is cut-off pill geometry, not missing label letters.

LVGL top pills begin at x=28 and y=20, where the circle only spans approximately x=53.7–186.3. At pill-center y=31 the left visible limit is approximately x=39.5, which is already inside that pill. Footer anchors similarly crowd the lower circular edge.

Use a circle-safe layout constraint for complete widget bounds, including corners, shadows, and text. For Arduino, reducing bottom pill width to 80 and placing them at x=38 and x=122 is a useful candidate to validate with the actual rounded shapes; allow additional inward margin for the bezel. Move LVGL pills inward/downward and center or stack the footer.

### U5 · P2 · Command labels have no overflow policy

[Command rows](C:/Users/haran/source/repos/smart_device_firmware/components/gui/gui_dashboard.cpp:232), [dynamic text](C:/Users/haran/source/repos/smart_device_firmware/components/gui/gui_dashboard.cpp:381)

The 160-pixel top row contains an unconstrained command label and timing label. The bottom row accepts a command message up to 63 characters into another fixed 160-pixel row. Labels have no allocated width, ellipsis mode, or separate detail view. Long commands/results exceed their parents and are clipped; a fixed flex row cannot make arbitrary text fit. The card's 52-pixel height with 8-pixel padding also leaves less vertical content space than its 16+20+2 pixels of rows and gap.

Reserve a fixed timing width; give command text the remaining width with ellipsis. Use a short status summary on the dashboard and expose full results in a detail view or console. Set card size from its content requirements. Test the longest command name, a 63-character error, and multi-line status output.

### U6 · P2 · Timing and heap widgets display constants

[Timing initialization](C:/Users/haran/source/repos/smart_device_firmware/components/gui/gui_dashboard.cpp:246), [heap fill](C:/Users/haran/source/repos/smart_device_firmware/components/gui/gui_dashboard.cpp:294), [discarded elapsed time](C:/Users/haran/source/repos/smart_device_firmware/components/gui/gui.cpp:67)

The timing label stays at `5 ms`: the result API discards `time_ms` and dashboard update never writes the label. The heap fill is always 28/40 pixels, independent of `state.free_heap`. These visual metrics are misleading. Carry measured elapsed time through UiState; give heap usage a defined denominator and update its fill, or omit the indicator.

## Pixel, clarity, and UI enhancement priorities

1. **Fix geometry before adding effects.** Use a 4-pixel spacing scale, measured text bounds, circle-safe insets, and one-pixel borders. The current doubled pill borders consume scarce space and do not provide antialiasing.
2. **Improve the Arduino clock grouping.** The bundled large font advances HH:MM about 85 pixels from x=54, while seconds start at x=162. That leaves roughly 23 pixels between the cursor end and seconds. Measure the full group and center it with a deliberate 4–6 pixel gap instead of independent cursor constants. This is a spacing improvement; no Arduino clock overlap was established.
3. **Reduce unnecessary repainting.** Arduino clears the entire 192×70 hero region and repaints cards/rings every second even when most values are unchanged. That clear alone represents 26,880 pixel bytes, about 54 ms of theoretical wire time at 4 MHz before drawing overhead. Cache state and repaint only dirty regions; consider a small composed clock buffer if memory allows. Measure flicker/tearing on hardware rather than promising that rectangle clears eliminate every artifact.
4. **Clarify connectivity state.** `WIFI ON` combines an associated hotspot client with an upstream connection. Show AP active/client count separately from internet/time-sync state. Reduce duplicate connectivity indicators unless each conveys something distinct.
5. **Complete setup guidance.** Serial messages tell the user to see the display for the hotspot password, but the dashboard renders SSID/IP without the password. Provide a deliberate provisioning screen or another working setup route.
6. **Use truthful status.** Replace the unmeasured `OPTIMAL (100)` score with actual health text. Distinguish unsynchronized uptime from local time; keep error meaning in text as well as color. Update or hide fixed demo values at startup.
7. **Validate both themes on glass.** Check secondary text, one-pixel strokes, and active/inactive colors at normal viewing distance. Use larger type for essential fields and move engineering telemetry to a secondary view if it competes with the clock.

## Recommended completion order

1. Restore the IDF build and make the supported SDK versions explicit.
2. Fix task/enclave exit, generation ownership, startup binding, and mutex queue integrity.
3. Correct caller attribution and validate all SVC inputs/outputs; route actual services through the gate.
4. Define and measure the supported timing/recovery contracts; align the architecture document with demonstrated behavior.
5. Fix LVGL locking, clock formatting, date overlap, circular clipping, command overflow, and constant metrics.
6. Capture both themes on the actual 240×240 panel with short/long text and state transitions. Check cold boot, splash replay, theme switch, Wi-Fi join/disconnect, BLE state changes, command failure, and 100+ hour uptime. Preserve image baselines for future UI changes.

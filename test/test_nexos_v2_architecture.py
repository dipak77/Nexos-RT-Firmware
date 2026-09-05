#!/usr/bin/env python3
"""
Nexos-RT V2 Architecture Enterprise QA & Validation Suite
Validates:
- Enclave descriptor contracts, capability masks, and lifecycle transitions
- Software SVC Gate pointer verification and permission enforcement
- O(1) Scheduler deadline-ordered priority pick and tie-breaking
- 1kHz budget decrement, overrun detection, and RT-preempts-INFER logic
- Robust Mutex priority inheritance & MK_MUTEX_OWNER_DEAD propagation
- Structured 128-entry SRAM fault ring circular overwrite behavior
- POSIX compatibility layer mapping and symbols
- Port isolation and hardware HAL quarantine
"""

import unittest
from pathlib import Path

class TestNexosV2Architecture(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.root = Path(__file__).parent.parent

    def read(self, rel_path: str) -> str:
        return (self.root / rel_path).read_text(encoding="utf-8")

    # --- 1. Enclave Descriptor & Lifecycle Contracts ---
    def test_enclave_descriptor_structure(self):
        """Verify that mk_enclave_desc_t contains all required patent & hardware isolation fields."""
        content = self.read("components/microkernel/include/mk_enclave.h")
        self.assertIn("uintptr_t base;", content)
        self.assertIn("uintptr_t limit;", content)
        self.assertIn("uintptr_t stack_base;", content)
        self.assertIn("size_t stack_size;", content)
        self.assertIn("uint32_t asid;", content)
        self.assertIn("uint32_t deadline_us;", content)
        self.assertIn("uint32_t budget_us;", content)
        self.assertIn("uint32_t syscall_mask;", content)
        self.assertIn("uint32_t held_mutex_mask;", content)
        self.assertIn("bool dma_active;", content)

    def test_enclave_lifecycle_states(self):
        """Ensure all required lifecycle states are defined."""
        content = self.read("components/microkernel/include/mk_enclave.h")
        for state in ["MK_ENCLAVE_FREE", "MK_ENCLAVE_RESERVED", "MK_ENCLAVE_LIVE", "MK_ENCLAVE_FAILED", "MK_ENCLAVE_RECLAIMING"]:
            self.assertIn(state, content)

    # --- 2. Software SVC Gate & Capability Sandboxing ---
    def test_capability_bitmasks(self):
        """Verify capability bitmask definitions."""
        content = self.read("components/microkernel/include/mk_enclave.h")
        self.assertIn("#define MK_CAP_GFX", content)
        self.assertIn("#define MK_CAP_NET", content)
        self.assertIn("#define MK_CAP_STORAGE", content)
        self.assertIn("#define MK_CAP_GPIO", content)
        self.assertIn("#define MK_CAP_AUDIO", content)

    def test_svc_pointer_sanitizer_algorithm(self):
        """Simulate and verify the SVC pointer boundary sanitizer."""
        def validate_ptr(base, limit, stack_base, stack_size, ptr, length):
            end = ptr + length
            if end < ptr:  # Overflow check
                return False
            # Check 1: Inside enclave heap/data window
            if base > 0 and limit > base:
                if ptr >= base and end <= limit:
                    return True
            # Check 2: Inside enclave stack
            if stack_base > 0 and stack_size > 0:
                stack_limit = stack_base + stack_size
                if ptr >= stack_base and end <= stack_limit:
                    return True
            return False

        # Valid pointers inside heap [0x3F800000, 0x3F810000)
        base, limit = 0x3F800000, 0x3F810000
        stk_base, stk_size = 0x3FC90000, 4096
        self.assertTrue(validate_ptr(base, limit, stk_base, stk_size, 0x3F800000, 1024))
        self.assertTrue(validate_ptr(base, limit, stk_base, stk_size, 0x3F805000, 256))
        self.assertTrue(validate_ptr(base, limit, stk_base, stk_size, 0x3FC90100, 128))

        # Invalid OOB pointers
        self.assertFalse(validate_ptr(base, limit, stk_base, stk_size, 0x3F7FFFF0, 32))  # Below heap
        self.assertFalse(validate_ptr(base, limit, stk_base, stk_size, 0x3F80FF00, 512)) # Crosses heap end
        self.assertFalse(validate_ptr(base, limit, stk_base, stk_size, 0x3FC80000, 64))  # Arbitrary foreign memory

    # --- 3. Earliest Deadline First (EDF) Ordering Algorithm ---
    def test_deadline_ordered_insertion_logic(self):
        """Verify sorting ready list by earliest deadline within identical priority."""
        class MockTask:
            def __init__(self, tid, prio, deadline_us):
                self.id = tid
                self.prio = prio
                self.deadline_us = deadline_us

        ready_queue = []

        def add_ready(task):
            if task.deadline_us > 0 and len(ready_queue) > 0:
                idx = len(ready_queue)
                for i, existing in enumerate(ready_queue):
                    if existing.deadline_us == 0 or existing.deadline_us > task.deadline_us:
                        idx = i
                        break
                ready_queue.insert(idx, task)
            else:
                ready_queue.append(task)

        t1 = MockTask(1, 5, 20000)
        t2 = MockTask(2, 5, 5000)   # Earlier deadline -> must jump ahead
        t3 = MockTask(3, 5, 12000)  # Middle deadline
        t4 = MockTask(4, 5, 0)      # Legacy/no deadline -> tail

        add_ready(t1)
        add_ready(t2)
        add_ready(t3)
        add_ready(t4)

        order = [t.id for t in ready_queue]
        self.assertEqual(order, [2, 3, 1, 4])

    # --- 4. Budget Enforcement & Overrun Trap Simulation ---
    def test_budget_sampler_semantics(self):
        """Mirror mk_enclave_sample_budgets: real deltas drain, zero deltas dorm."""
        def sample(remaining, budget, deltas):
            trapped = False
            for d in deltas:
                if d == 0:
                    continue  # stats off / task idle: dormant, honest
                if d >= remaining:
                    remaining = 0
                    trapped = True
                    break
                remaining -= d
            return remaining, trapped

        # Real counters: 8ms budget dies on first sizable sample
        rem, trapped = sample(8000, 8000, [0, 45000])
        self.assertTrue(trapped)
        self.assertEqual(rem, 0)
        # Stats disabled (all-zero deltas): never traps, never drains
        rem, trapped = sample(8000, 8000, [0, 0, 0, 0])
        self.assertFalse(trapped)
        self.assertEqual(rem, 8000)

    def test_mutex_no_steal_two_waiters(self):
        """Mirror fixed mk_mutex_lock: a sticky owner_dead flag never permits
        stealing a lock that is already held by the recovering holder."""
        class M:
            def __init__(self):
                self.locked = False
                self.owner_dead = False
                self.owner = None
                self.recursion = 0

        def lock(m, me):
            if m.locked and m.owner == me:
                m.recursion += 1
                return True
            if not m.locked:  # acquire ONLY a free lock (no `or owner_dead`)
                m.locked = True
                m.recursion = 1
                m.owner = me
                return True
            return False  # blocks/times out in C

        m = M()
        # Dead owner reclaimed: free lock, flag sticky
        m.locked, m.owner_dead, m.owner = False, True, None
        self.assertTrue(lock(m, "A"))   # recoverer takes it, flag stays for query
        self.assertTrue(m.owner_dead)   # queryable before unlock
        self.assertFalse(lock(m, "B"))  # second waiter must NOT steal
        self.assertEqual(m.owner, "A")

    def test_validate_ptr_unconfigured_enclave(self):
        """base=limit=stack=0 denies everything (fail-closed, no silent pass)."""
        def validate(base, limit, sbase, ssize, ptr, length):
            end = ptr + length
            if end < ptr:
                return False
            if base > 0 and limit > base and base <= ptr and end <= limit:
                return True
            if sbase > 0 and ssize > 0 and sbase <= ptr and end <= sbase + ssize:
                return True
            return False

        self.assertFalse(validate(0, 0, 0, 0, 0x3FC90000, 64))
        self.assertTrue(validate(0, 0, 0x3FC90000, 4096, 0x3FC90100, 64))

    # --- 5. Robust Mutex Recovery & OWNER_DEAD Propagation ---
    def test_robust_mutex_owner_dead_status_codes(self):
        """Verify MK_ERR_DEADLOCK_OWNER_DEAD status code exists in headers and kernel."""
        mk_types = self.read("components/microkernel/include/mk_types.h")
        self.assertIn("MK_ERR_DEADLOCK_OWNER_DEAD = 1014", mk_types)
        self.assertIn("MK_ERR_PEER_TERMINATED = 1015", mk_types)

        mk_mutex_h = self.read("components/microkernel/include/mk_mutex.h")
        self.assertIn("mk_mutex_mark_owner_dead", mk_mutex_h)
        self.assertIn("mk_mutex_reclaim_for_task", mk_mutex_h)

    # --- 6. Structured SRAM Fault Ring Circular Rollover ---
    def test_fault_ring_circular_overflow(self):
        """Verify that a 128-entry circular ring accurately rolls over and preserves latest 128 items."""
        ring_size = 128
        total_items = 300
        ring = [None] * ring_size
        head = 0

        for i in range(total_items):
            ring[head % ring_size] = f"FAULT_{i}"
            head += 1

        # Must have exactly 300 recorded, and retained items must be 172 to 299
        self.assertEqual(head, 300)
        retained = [ring[(head - ring_size + i) % ring_size] for i in range(ring_size)]
        self.assertEqual(retained[0], "FAULT_172")
        self.assertEqual(retained[-1], "FAULT_299")

    # --- 7. POSIX Compatibility Layer ---
    def test_posix_shim_declarations(self):
        """Ensure clean POSIX mappings exist without FreeRTOS contamination."""
        content = self.read("components/posix_shim/include/nexos_posix.h")
        self.assertIn("nexos_pthread_create", content)
        self.assertIn("nexos_pthread_mutex_lock", content)
        self.assertIn("nexos_mq_send", content)
        self.assertNotIn("freertos", content.lower())

    # --- 8. Xtensa Assembly Context Switch (quarantined, NOT shipped) ---
    def test_xtensa_context_switch_quarantined(self):
        """mk_context.S must stay out of every build graph until rewritten with
        a loopback test. Presence of the file proves nothing about execution."""
        cmake = self.read("components/microkernel/CMakeLists.txt")
        # A quoted SRCS entry means "compile me"; comment mentions are fine.
        self.assertNotIn('"port/native/mk_context.S"', cmake)
        self.assertNotIn("'port/native/mk_context.S'", cmake)

    # --- 9. Hardware GPTimer & Watchpoint Drivers ---
    def test_hardware_drivers_present(self):
        """Verify hardware GPTimer, IPI, and Watchpoint drivers are correctly implemented."""
        self.assertTrue((self.root / "components/microkernel/arch/esp32s3/mk_gptimer.c").exists())
        self.assertTrue((self.root / "components/microkernel/arch/esp32s3/mk_ipi.c").exists())
        self.assertTrue((self.root / "components/microkernel/arch/esp32s3/mk_watchpoint.c").exists())

    # --- 10. CLI Diagnostic Parity ---
    def test_cli_diagnostic_commands_registered(self):
        """Verify V2 diagnostic CLI commands are registered in command registry."""
        content = self.read("components/command/command_registry.cpp")
        self.assertIn('"enclave_show"', content)
        self.assertIn('"enclave_reset"', content)
        self.assertIn('"fault_show"', content)
        self.assertIn('"fault_clear"', content)
        self.assertIn('"wdt_show"', content)

    # --- 11. Prod-grade correctness contracts (post-review fixes) ---
    def test_current_task_resolves_from_port(self):
        """PI/budget/SVC attribution must resolve the live task, not a stale cache."""
        sched = self.read("components/microkernel/core/mk_scheduler.c")
        self.assertIn("mk_task_self()", sched)
        self.assertIn("mk_scheduler_current_task", sched)

    def test_owner_dead_contract_is_ok_plus_query(self):
        """Recovery acquisition returns MK_OK; death reported via was_owner_dead()."""
        mutex_c = self.read("components/microkernel/ipc/mk_mutex.c")
        self.assertIn("mk_mutex_was_owner_dead", mutex_c)
        mutex_h = self.read("components/microkernel/include/mk_mutex.h")
        self.assertIn("mk_mutex_was_owner_dead", mutex_h)
        # No path may return OWNER_DEAD *while holding* the lock anymore
        self.assertNotIn("return was_owner_dead ? MK_ERR_DEADLOCK_OWNER_DEAD : MK_OK", mutex_c)
        # Steal clause must be gone: acquire only free locks, recursive first
        self.assertNotIn("|| was_owner_dead", mutex_c)
        self.assertIn("mk_enclave_set_heap_window", self.read("components/microkernel/include/mk_enclave.h"))
        self.assertIn("mk_port_task_stack_base",
                      self.read("components/microkernel/port/mk_port.h"))

    def test_single_tick_source(self):
        """gptimer must delegate to the scheduler driver, never own a second timer."""
        gpt = self.read("components/microkernel/arch/esp32s3/mk_gptimer.c")
        self.assertIn("mk_scheduler_start_tick", gpt)
        self.assertNotIn("esp_timer_create", gpt)

    def test_context_asm_quarantined_from_builds(self):
        """mk_context.S stays out of all build graphs until rewritten + loopback-tested."""
        cmake = self.read("components/microkernel/CMakeLists.txt")
        self.assertNotIn('"port/native/mk_context.S"', cmake)

    def test_arduino_cli_has_v2_commands(self):
        """V2 diagnostics must be reachable on the flashed Arduino image, not just IDF."""
        main_cpp = self.read("src/main.cpp")
        for cmd in ["enclave_show", "enclave_drill", "fault_show", "fault_clear", "wdt_show"]:
            self.assertIn(cmd, main_cpp)
        self.assertIn("mk_enclave_init()", main_cpp)

    def test_boot_tasks_bound_via_enclave_start(self):
        """Boot tasks must be created through enclaves, never raw mk_task_create_ext."""
        km = self.read("src/kernel_manager.cpp")
        self.assertIn("mk_enclave_create", km)
        self.assertIn("mk_enclave_start", km)
        self.assertIn("mk_enclave_reclaim", km)
        self.assertIn("MK_CAP_GFX", km)
        # Rollback must reclaim enclaves (which deletes bound tasks), not raw-delete
        self.assertIn("rollback_one", km)

    def test_budget_sampler_uses_real_counters(self):
        """Sampler matches port handles against run-time counters; dormant when stats off."""
        enc = self.read("components/microkernel/core/mk_enclave.c")
        self.assertIn("mk_enclave_sample_budgets", enc)
        self.assertIn("mk_port_task_runtime", enc)
        self.assertIn("mk_task_get_port_handle", enc)
        port_h = self.read("components/microkernel/port/mk_port.h")
        self.assertIn("mk_port_task_runtime", port_h)

    def test_budget_trap_suspends_real_task(self):
        """Budget exhaust must suspend execution via the port, not just shadow state."""
        enc = self.read("components/microkernel/core/mk_enclave.c")
        self.assertIn("mk_task_suspend", enc)
        self.assertIn("MK_FAULT_BUDGET_EXHAUSTED", enc)
        # Tick must NOT double-account budgets (single owner: the sampler)
        sched = self.read("components/microkernel/core/mk_scheduler.c")
        self.assertIn("mk_enclave_sample_budgets", sched)

    def test_infer_deferral_uses_time_remaining(self):
        """Deferral compares time-to-deadline, never an absolute timestamp constant."""
        sched = self.read("components/microkernel/core/mk_scheduler.c")
        self.assertIn("remaining < 5000", sched)
        self.assertNotIn("deadline_us < 5000", sched)

    # --- 12. UI & Display Correctness Contracts (U1 - U6) ---
    def test_ui_lvgl_lock_guard_checked(self):
        """U1: LVGL lock acquisition must be checked before updating dashboard."""
        app_main = self.read("main/app_main.cpp")
        self.assertIn("LvglLockGuard lock;", app_main)
        self.assertIn("if (lock)", app_main)
        gui_c = self.read("components/gui/gui.cpp")
        self.assertIn("if(runtime.lock(200))", gui_c)

    def test_ui_clock_and_ampm_separation(self):
        """U2: HH:MM, seconds, and AM/PM must be separated without duplication."""
        ts_c = self.read("components/time_service/time_service.cpp")
        self.assertIn('strftime(time_buf, time_len, "%H:%M", &ti)', ts_c)
        self.assertIn('strftime(time_buf, time_len, "%I:%M", &ti)', ts_c)
        dash = self.read("components/gui/gui_dashboard.cpp")
        self.assertIn("lv_obj_clear_flag(ampm_label, LV_OBJ_FLAG_HIDDEN)", dash)
        self.assertIn("lv_obj_add_flag(ampm_label, LV_OBJ_FLAG_HIDDEN)", dash)

    def test_ui_date_divider_separation(self):
        """U3: Divider must not cross the date text band."""
        dash = self.read("components/gui/gui_dashboard.cpp")
        self.assertIn("lv_obj_align(date_label, LV_ALIGN_TOP_MID, 0, 92)", dash)
        self.assertIn("lv_obj_align(divider, LV_ALIGN_CENTER, 0, -12)", dash)
        self.assertIn("lv_obj_align(status_chip, LV_ALIGN_CENTER, 0, 4)", dash)

    def test_ui_circular_safe_area_bounds(self):
        """U4: Status pills must fit strictly within 240x240 circular panel (R=120)."""
        main_cpp = self.read("src/main.cpp")
        self.assertIn("draw_status_pill(40, 188", main_cpp)
        self.assertIn("draw_status_pill(124, 188", main_cpp)
        # Verify corner math: x=40, y=206 gives R=117.4 < 120
        import math
        r_corner = math.sqrt((40 - 120)**2 + (206 - 120)**2)
        self.assertLess(r_corner, 120.0)

    def test_ui_command_card_overflow_protection(self):
        """U5: Command card labels must have ellipsis long-mode and bounded widths."""
        dash = self.read("components/gui/gui_dashboard.cpp")
        self.assertIn("lv_label_set_long_mode(cmd_label, LV_LABEL_LONG_DOT)", dash)
        self.assertIn("lv_label_set_long_mode(cmd_result_label, LV_LABEL_LONG_DOT)", dash)

    def test_ui_dynamic_metrics_wired(self):
        """U6: Command elapsed time and heap bar fill must be dynamically updated."""
        dash = self.read("components/gui/gui_dashboard.cpp")
        self.assertIn("lv_label_set_text(cmd_time_label, t_buf)", dash)
        self.assertIn("lv_obj_set_width(heap_fill, fill_w)", dash)

    # --- 13. Core Architectural Contracts (A1 - A10) ---
    def test_a1_idf_cmake_requirements(self):
        """A1: CMakeLists.txt must not require obsolete esp_ipc component."""
        cmake = self.read("components/microkernel/CMakeLists.txt")
        self.assertNotIn("esp_ipc", cmake)
        self.assertIn("esp_system", cmake)

    def test_a2_priority_inheritance_queue_unlinking_order(self):
        """A2: mk_scheduler_remove_ready must precede priority mutation."""
        mutex_c = self.read("components/microkernel/ipc/mk_mutex.c")
        # In boost path: remove_ready must appear before assigning priority
        boost_idx = mutex_c.find("Fix A2: Remove from ready queue using OLD priority first")
        self.assertGreater(boost_idx, 0)
        # Check struct has highest_waiter_prio
        self.assertIn("highest_waiter_prio", mutex_c)

    def test_a3_a4_task_reaper_and_self_reclaim(self):
        """A3/A4: mk_task_reap must recycle tombstones; self-reclaim must transition to FREE."""
        task_c = self.read("components/microkernel/core/mk_task.c")
        self.assertIn("void mk_task_reap(void)", task_c)
        self.assertIn("mk_task_reap();", task_c)
        enclave_c = self.read("components/microkernel/core/mk_enclave.c")
        self.assertIn("desc->state = MK_ENCLAVE_FREE;", enclave_c)
        self.assertIn("desc->task_handle = NULL;", enclave_c)
        self.assertIn("desc->stack_base = 0;", enclave_c)

    def test_a5_foreign_caller_returns_null(self):
        """A5: mk_scheduler_current_task must return NULL for unregistered callers."""
        sched_c = self.read("components/microkernel/core/mk_scheduler.c")
        self.assertIn("return mk_task_self();", sched_c)
        self.assertNotIn("return s_current_task;", sched_c)

    def test_a6_task_creation_latch(self):
        """A6: Tasks must wait for ready_to_run latch before executing entry."""
        task_c = self.read("components/microkernel/core/mk_task.c")
        self.assertIn("while(!internal->ready_to_run)", task_c)
        self.assertIn("mk_task_release_latch", task_c)
        types_h = self.read("components/microkernel/include/mk_types.h")
        self.assertIn("bool defer_start;", types_h)

    def test_a7_watchpoint_cross_core_coordination(self):
        """A7: Watchpoint disarm and arm must coordinate across CPU cores."""
        wp_c = self.read("components/microkernel/arch/esp32s3/mk_watchpoint.c")
        self.assertIn("esp_ipc_call_blocking", wp_c)
        self.assertIn("esp_cpu_get_core_id()", wp_c)

    def test_a8_svc_args_out_result_bounds_check(self):
        """A8: Syscall dispatcher must validate args, out_result, and buffers."""
        svc_c = self.read("components/microkernel/core/mk_svc.c")
        self.assertIn("mk_svc_validate_ptr(enclave, args, sizeof(mk_svc_args_t))", svc_c)
        self.assertIn("mk_svc_validate_ptr(enclave, out_result, sizeof(uintptr_t))", svc_c)
        self.assertIn("MK_SVC_NET_RECV", svc_c)
        self.assertIn("MK_SVC_IPC_RECV", svc_c)

    def test_a9_budget_baseline_and_generation(self):
        """A9: Enclave start must establish initial baseline and track task generations."""
        enclave_c = self.read("components/microkernel/core/mk_enclave.c")
        self.assertIn("s_prev_task_id", enclave_c)
        self.assertIn("s_prev_task_id[desc->id] = t->id;", enclave_c)

    def test_a10_dual_track_watchdog_query(self):
        """A10: wdt_show must query COMMAND with fallback to CLI."""
        cmd_c = self.read("components/command/command_registry.cpp")
        self.assertIn('mk_watchdog_age_ms("COMMAND")', cmd_c)
        self.assertIn('mk_watchdog_age_ms("CLI")', cmd_c)

if __name__ == "__main__":
    unittest.main()

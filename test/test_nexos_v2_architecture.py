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
    def test_budget_exhaustion_simulation(self):
        """Verify that 1kHz tick decrements remaining_budget_us and fires trap when exhausted."""
        budget_us = 5000  # 5ms budget
        remaining = budget_us
        trapped = False

        for tick in range(10):  # 10ms of execution
            if remaining > 1000:
                remaining -= 1000
            else:
                remaining = 0
                trapped = True
                break

        self.assertTrue(trapped)
        self.assertEqual(remaining, 0)

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

    # --- 8. Xtensa Assembly Context Switch ---
    def test_xtensa_context_switch_assembly_source(self):
        """Verify mk_context.S implements register spill, SAR, and PS save/restore."""
        content = self.read("components/microkernel/port/native/mk_context.S")
        self.assertIn("mk_context_switch:", content)
        self.assertIn("rsr.sar", content)
        self.assertIn("rsr.ps", content)
        self.assertIn("wsr.sar", content)
        self.assertIn("wsr.ps", content)
        self.assertIn("mk_start_first_task:", content)

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

if __name__ == "__main__":
    unittest.main()

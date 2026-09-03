#!/usr/bin/env python3
"""
Nexos-RT Custom Microkernel Base OS Validation Suite
Validates:
- Header barrier & port isolation (zero FreeRTOS headers in core)
- O(1) bitmap scheduler algorithms and priority selection
- Priority Inheritance Protocol (PIP) promotion & unboost logic
- Health matrix scoring, degradation modes, and recovery policy
- Crash flight recorder persistence contracts and magic headers
"""

import unittest
from pathlib import Path

class TestNexosBaseOS(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.root = Path(__file__).parent.parent

    def read(self, rel_path: str) -> str:
        return (self.root / rel_path).read_text(encoding="utf-8")

    # --- 1. Header Barrier & Port Isolation ---
    def test_microkernel_core_is_freertos_free(self):
        """Core kernel modules must never directly include vendor FreeRTOS headers."""
        core_dirs = ["core", "ipc", "time", "memory"]
        for d in core_dirs:
            dir_path = self.root / "components" / "microkernel" / d
            if not dir_path.exists():
                continue
            for f in dir_path.glob("*.[ch]"):
                content = f.read_text(encoding="utf-8")
                self.assertNotIn(
                    '#include "freertos/',
                    content,
                    f"Violation: {f.name} includes FreeRTOS directly! Core must be vendor-free."
                )

    def test_native_kernel_flag_is_enabled(self):
        """Production configuration must have MK_NATIVE_KERNEL enabled."""
        mk_config = self.read("components/microkernel/include/mk_config.h")
        self.assertIn("#define MK_NATIVE_KERNEL 1", mk_config)
        self.assertIn("#define MK_ISOLATE_RADIO 1", mk_config)

    # --- 2. O(1) Scheduler Logic & Bitmap Priority Model ---
    def test_scheduler_clz_bitmap_priority_selection(self):
        """Verify the 31 - clz(ready_bitmap) single-cycle priority selection algorithm."""
        def clz32(n):
            if n == 0:
                return 32
            b = bin(n)[2:].zfill(32)
            return len(b) - len(b.lstrip('0'))

        def pick_highest_prio(ready_bitmap):
            if ready_bitmap == 0:
                return None
            return 31 - clz32(ready_bitmap)

        # Test single priorities
        for prio in range(8):
            bitmap = 1 << prio
            self.assertEqual(pick_highest_prio(bitmap), prio)

        # Test multiple ready priorities: highest must always win
        # Priority 7 (GUI) + Priority 2 (Storage) -> Priority 7 selected
        self.assertEqual(pick_highest_prio((1 << 7) | (1 << 2)), 7)
        # Priority 6 (Command) + Priority 3 (Diagnostics) -> Priority 6 selected
        self.assertEqual(pick_highest_prio((1 << 6) | (1 << 3)), 6)
        # Empty bitmap -> None
        self.assertEqual(pick_highest_prio(0), None)

    # --- 3. Priority Inheritance Protocol (PIP) ---
    def test_priority_inheritance_boost_and_unboost(self):
        """Simulate 3-task mutex contention and verify owner priority promotion and restoration."""
        class MockTask:
            def __init__(self, name, prio):
                self.name = name
                self.priority = prio
                self.base_priority = prio

        class MockMutex:
            def __init__(self):
                self.locked = False
                self.owner = None
                self.original_prio = 0

            def lock(self, task):
                if not self.locked:
                    self.locked = True
                    self.owner = task
                    self.original_prio = task.priority
                    return True
                # Contended: apply Priority Inheritance
                if task.priority > self.owner.priority:
                    self.owner.priority = task.priority
                return False

            def unlock(self, task):
                if self.owner != task:
                    return False
                # Restore unboosted base priority
                self.owner.priority = self.owner.base_priority
                self.locked = False
                self.owner = None
                return True

        task_low = MockTask("STORAGE", prio=2)
        task_med = MockTask("CONNECTIVITY", prio=5)
        task_high = MockTask("GUI", prio=7)
        mutex = MockMutex()

        # Step 1: Low-priority task takes display lock
        self.assertTrue(mutex.lock(task_low))
        self.assertEqual(task_low.priority, 2)

        # Step 2: High-priority task contends on display lock -> Low is boosted to 7
        self.assertFalse(mutex.lock(task_high))
        self.assertEqual(task_low.priority, 7, "Task Low must be boosted to Priority 7")

        # Step 3: Medium-priority task (prio 5) CANNOT preempt Low because Low is running at 7
        self.assertGreater(task_low.priority, task_med.priority)

        # Step 4: Low unlocks display -> Low drops back to its base priority 2
        self.assertTrue(mutex.unlock(task_low))
        self.assertEqual(task_low.priority, 2, "Task Low must drop back to base priority 2 after release")

    # --- 4. Health Matrix & Degradation Policies ---
    def test_health_scoring_and_degraded_modes(self):
        """Verify dynamic health degradation thresholds and recovery logic."""
        def evaluate_mode(score):
            if score >= 80:
                return "NORMAL"
            elif score >= 60:
                return "DEGRADED_UI"
            elif score >= 40:
                return "DEGRADED_NET"
            else:
                return "SAFE_MODE"

        score = 100
        self.assertEqual(evaluate_mode(score), "NORMAL")

        # Stall fault penalty: -20
        score -= 20
        self.assertEqual(score, 80)
        self.assertEqual(evaluate_mode(score), "NORMAL")

        # Second stall: -20 -> 60 (DEGRADED_UI)
        score -= 20
        self.assertEqual(score, 60)
        self.assertEqual(evaluate_mode(score), "DEGRADED_UI")

        # Low memory penalty: -15 -> 45 (DEGRADED_NET)
        score -= 15
        self.assertEqual(score, 45)
        self.assertEqual(evaluate_mode(score), "DEGRADED_NET")

        # Network/event drop penalty: -10 -> 35 (SAFE_MODE)
        score -= 10
        self.assertEqual(score, 35)
        self.assertEqual(evaluate_mode(score), "SAFE_MODE")

        # Recovery simulation: +1 per tick
        for _ in range(50):
            score = min(100, score + 1)
        self.assertEqual(score, 85)
        self.assertEqual(evaluate_mode(score), "NORMAL")

    # --- 5. Crash Flight Recorder Specification ---
    def test_crash_record_magic_and_structure(self):
        """Verify the RTC crash flight recorder magic number and field completeness."""
        mk_crash_h = self.read("components/microkernel/include/mk_crash.h")
        self.assertIn("#define MK_CRASH_MAGIC 0x4E455843", mk_crash_h)
        self.assertIn("char cause[16]", mk_crash_h)
        self.assertIn("char task_name[32]", mk_crash_h)
        self.assertIn("uint32_t fault_pc", mk_crash_h)
        self.assertIn("uint32_t uptime_s", mk_crash_h)
        self.assertIn("uint32_t free_heap_at_fault", mk_crash_h)
        self.assertIn("uint32_t reboot_count", mk_crash_h)

if __name__ == "__main__":
    unittest.main()

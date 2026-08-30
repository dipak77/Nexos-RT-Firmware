#!/usr/bin/env python3
"""
Unit test suite for Smart Device Firmware platform.
Tests command parsing, state machines, microkernel metrics, and configuration presets.
"""

import sys
import unittest
from pathlib import Path

# Add simulator to path
sys.path.insert(0, str(Path(__file__).parent.parent / "simulator"))
from run_simulator import DeviceState, CommandEngine, FW_VERSION, KERNEL_VERSION

class TestSmartDevicePlatform(unittest.TestCase):
    def setUp(self):
        self.state = DeviceState()
        self.engine = CommandEngine(self.state)

    def test_version_command(self):
        res = self.engine.execute("version")
        self.assertEqual(res["status"], "SUCCESS")
        self.assertIn(FW_VERSION, res["message"])
        self.assertIn(KERNEL_VERSION, res["message"])

    def test_help_command(self):
        res = self.engine.execute("help")
        self.assertEqual(res["status"], "SUCCESS")
        self.assertIn("Available Commands", res["message"])
        self.assertIn("wifi connect", res["message"])

    def test_status_command(self):
        res = self.engine.execute("status")
        self.assertEqual(res["status"], "SUCCESS")
        self.assertIn("SYSTEM STATUS", res["message"])
        self.assertIn("GC9A01", res["message"])

    def test_wifi_lifecycle(self):
        # Initial state
        self.assertFalse(self.state.wifi_connected)
        
        # Scan
        scan_res = self.engine.execute("wifi scan")
        self.assertEqual(scan_res["status"], "SUCCESS")
        self.assertIn("Scan results", scan_res["message"])

        # Connect
        conn_res = self.engine.execute("wifi connect TestAP SecretPass")
        self.assertEqual(conn_res["status"], "SUCCESS")
        self.assertTrue(self.state.wifi_connected)
        self.assertEqual(self.state.wifi_ssid, "TestAP")
        self.assertTrue(len(self.state.wifi_ip) > 0)

        # Status check
        st_res = self.engine.execute("wifi status")
        self.assertEqual(st_res["status"], "SUCCESS")
        self.assertIn("CONNECTED", st_res["message"])
        self.assertIn("TestAP", st_res["message"])

    def test_ble_commands(self):
        res = self.engine.execute("ble status")
        self.assertEqual(res["status"], "SUCCESS")
        self.assertIn("SmartDisplay-BLE", res["message"])

        stop_res = self.engine.execute("ble stop")
        self.assertEqual(stop_res["status"], "SUCCESS")
        self.assertFalse(self.state.ble_advertising)

        start_res = self.engine.execute("ble start")
        self.assertEqual(start_res["status"], "SUCCESS")
        self.assertTrue(self.state.ble_advertising)

    def test_time_service(self):
        res = self.engine.execute("time sync")
        self.assertEqual(res["status"], "SUCCESS")
        self.assertTrue(self.state.time_synced)

        t_res = self.engine.execute("time status")
        self.assertEqual(t_res["status"], "SUCCESS")
        self.assertIn("IST-5:30", t_res["message"])

    def test_display_brightness(self):
        res = self.engine.execute("display brightness 75")
        self.assertEqual(res["status"], "SUCCESS")
        self.assertEqual(self.state.brightness, 75)

        invalid_res = self.engine.execute("display brightness 200")
        self.assertEqual(invalid_res["status"], "FAILED")

    def test_self_test_diagnostic(self):
        res = self.engine.execute("self-test")
        self.assertEqual(res["status"], "SUCCESS")
        self.assertIn("DIAGNOSTIC SELF TEST", res["message"])
        self.assertIn("GC9A01 Display", res["message"])

    def test_kernel_diagnostics(self):
        res_kstatus = self.engine.execute("kernel status")
        self.assertEqual(res_kstatus["status"], "SUCCESS")
        self.assertIn("Microkernel", res_kstatus["message"])
        self.assertIn("RUNNING", res_kstatus["message"])

        res_ktasks = self.engine.execute("kernel tasks")
        self.assertEqual(res_ktasks["status"], "SUCCESS")
        self.assertIn("GUI", res_ktasks["message"])
        self.assertIn("COMMAND", res_ktasks["message"])

        res_kstats = self.engine.execute("kernel stats")
        self.assertEqual(res_kstats["status"], "SUCCESS")
        self.assertIn("uptime_ms", res_kstats["message"])

    def test_reboot_and_reset(self):
        self.engine.execute("wifi connect TempAP 1234")
        self.assertTrue(self.state.wifi_connected)

        res_reset = self.engine.execute("factory_reset")
        self.assertEqual(res_reset["status"], "SUCCESS")
        self.assertFalse(self.state.wifi_connected)
        self.assertEqual(self.state.wifi_ssid, "")

if __name__ == "__main__":
    unittest.main()

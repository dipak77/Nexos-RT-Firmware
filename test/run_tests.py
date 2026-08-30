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
        self.assertIn("Nexos-RT", res_kstatus["message"])
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


class TestFirmwareContracts(unittest.TestCase):
    """Regression checks for hardware and flash-layout failures found on-device."""

    @classmethod
    def setUpClass(cls):
        cls.root = Path(__file__).parent.parent

    def read(self, relative_path):
        return (self.root / relative_path).read_text(encoding="utf-8")

    def test_display_uses_safe_supply_and_explicit_control_pins(self):
        source = self.read("src/main.cpp")
        self.assertIn("#define TFT_CS    9", source)
        self.assertIn("#define TFT_RST   14", source)
        self.assertIn("#define TFT_SPI_HZ 2000000U", source)
        self.assertIn("SPI.begin(TFT_SCLK, -1, TFT_MOSI, -1)", source)
        self.assertIn("VCC=3V3 only", source)
        self.assertNotIn("VCC=5V", source)

    def test_both_builds_use_the_same_8mb_ota_layout(self):
        pio = self.read("platformio.ini")
        self.assertEqual(pio.count("board_build.partitions = partitions_8mb_ota.csv"), 2)
        sdkconfig = self.read("sdkconfig.defaults")
        self.assertIn('CONFIG_PARTITION_TABLE_FILENAME="partitions_8mb_ota.csv"', sdkconfig)
        table = self.read("partitions_8mb_ota.csv")
        self.assertIn("ota_0,      app,  ota_0,   0x20000,  0x300000", table)
        self.assertIn("ota_1,      app,  ota_1,   0x320000, 0x300000", table)

    def test_release_tools_flash_the_app_at_ota0(self):
        flasher = self.read("release/flash_device.ps1")
        packager = self.read("tools/generate_release_binaries.py")
        self.assertIn('@("0x20000", $appBin)', flasher)
        self.assertIn("APP_OFFSET = 0x20000", packager)
        self.assertNotIn('@("0x10000", $appBin)', flasher)

    def test_flasher_fails_safely_and_does_not_guess_a_port(self):
        flasher = self.read("release/flash_device.ps1")
        batch = self.read("release/flash_device.bat")
        self.assertIn("Could not uniquely identify", flasher)
        self.assertIn("VID_10C4&PID_EA60", flasher)
        self.assertNotIn('$Port = "COM7"', flasher)
        self.assertIn("exit 1", flasher)
        self.assertIn("v1.2.0", batch)

    def test_versions_are_synchronized(self):
        version = self.read("VERSION").strip()
        self.assertEqual(version, "1.2.0")
        self.assertEqual(FW_VERSION, version)
        self.assertEqual(KERNEL_VERSION, version)
        self.assertIn('#define APP_VERSION_STRING "1.2.0"', self.read("components/common/include/app_version.h"))
        self.assertIn('#define MK_CONFIG_VERSION_STRING "1.2.0"', self.read("components/microkernel/include/mk_config.h"))

    def test_application_does_not_bypass_microkernel_scheduler_api(self):
        excluded = ("components/microkernel/port/", "components/microkernel/arch/")
        offenders = []
        for source_root in ("src", "main", "components"):
            for path in (self.root / source_root).rglob("*"):
                if path.suffix not in {".c", ".cc", ".cpp", ".h", ".hpp"}:
                    continue
                relative = path.relative_to(self.root).as_posix()
                if relative.startswith(excluded):
                    continue
                text = path.read_text(encoding="utf-8")
                if '#include "freertos/' in text or "xPortGetCoreID(" in text:
                    offenders.append(relative)
        self.assertEqual(offenders, [])

    def test_ota_uses_certificate_bundle_without_uninitialized_global_store(self):
        source = self.read("components/ota/ota_service.cpp")
        self.assertIn("http_cfg.crt_bundle_attach = esp_crt_bundle_attach", source)
        self.assertIn("http_cfg.use_global_ca_store = false", source)

    def test_i2c_uses_supported_idf_master_driver(self):
        header = self.read("components/device_hal/include/i2c_hal.h")
        source = self.read("components/device_hal/i2c_hal.cpp")
        self.assertIn('#include "driver/i2c_master.h"', header)
        self.assertIn("i2c_new_master_bus", source)
        self.assertIn("i2c_master_probe", source)
        self.assertNotIn("i2c_driver_install", source)

    def test_production_build_does_not_compile_lvgl_examples_or_demos(self):
        defaults = self.read("sdkconfig.defaults")
        self.assertIn("CONFIG_LV_BUILD_EXAMPLES=n", defaults)
        self.assertIn("CONFIG_LV_BUILD_DEMOS=n", defaults)

if __name__ == "__main__":
    unittest.main()

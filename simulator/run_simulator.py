#!/usr/bin/env python3
"""
Smart Device Platform - Desktop Simulator
Simulates GC9A01 240x240 Round TFT Display + Glassmorphism UI + Microkernel Command Engine.
"""

import sys
import time
import json
import random
import datetime
import threading
from typing import Dict, Any, List, Tuple

# Firmware Defaults
FW_VERSION = "1.2.0"
HW_VERSION = "S3-DK-V1.1"
KERNEL_VERSION = "1.2.0"
MODEL = "S3-GC9A01"
BUILD_TIME = datetime.datetime.now().strftime("%b %d %Y %H:%M:%S")

class DeviceState:
    def __init__(self):
        self.wifi_enabled = True
        self.wifi_connected = False
        self.wifi_ssid = ""
        self.wifi_ip = ""
        self.wifi_rssi = -65
        
        self.ble_enabled = True
        self.ble_advertising = True
        self.ble_connected = False
        self.ble_name = "SmartDisplay-BLE"
        
        self.time_synced = False
        self.timezone = "IST-5:30"
        self.time_24h = False
        
        self.brightness = 80
        self.system_status = "NO WIFI"
        self.latest_command = "SYSTEM READY"
        self.command_status = "SUCCESS"  # SUCCESS, FAILED, BUSY
        self.command_message = "✓ PASS • System Initialized"
        self.command_time_ms = 4
        
        self.uptime_sec = 0
        self.free_heap = 320 * 1024
        self.min_free_heap = 280 * 1024
        self.context_switches = 1420
        self.active_tasks = 4
        self.boot_time = time.time()

    def get_time_strings(self) -> Tuple[str, str, str, str]:
        now = datetime.datetime.now()
        if self.time_24h:
            time_main = now.strftime("%H:%M")
            sec_str = now.strftime(":%S")
            ampm = ""
        else:
            time_main = now.strftime("%I:%M")
            sec_str = now.strftime(":%S")
            ampm = now.strftime("%p")
        date_str = now.strftime("%d %b %Y").upper()
        return time_main, sec_str, ampm, date_str

    def update_ticks(self):
        self.uptime_sec = int(time.time() - self.boot_time)
        self.context_switches += random.randint(5, 25)
        if self.wifi_connected and self.time_synced:
            self.system_status = "SYSTEM OK"
        elif self.wifi_connected:
            self.system_status = "SYNCING TIME"
        else:
            self.system_status = "NO WIFI"

class CommandEngine:
    def __init__(self, state: DeviceState):
        self.state = state

    def execute(self, line: str) -> Dict[str, Any]:
        line = line.strip()
        if not line:
            return {"status": "INVALID", "message": "Empty command", "time_ms": 0}
            
        t0 = time.time()
        tokens = line.split()
        cmd = tokens[0].lower()
        args = tokens[1:]
        
        # Compound commands
        if len(tokens) >= 2:
            compound = f"{tokens[0]}_{tokens[1]}".lower()
            if compound in ["wifi_status", "wifi_scan", "wifi_connect", "ble_status", "ble_start", "ble_stop",
                            "time_status", "time_sync", "display_test", "display_brightness", "self_test",
                            "kernel_status", "kernel_tasks", "kernel_stats", "system_info", "system_status",
                            "factory_reset"]:
                cmd = compound
                args = tokens[2:]

        result_status = "SUCCESS"
        result_msg = ""

        if cmd in ["help", "?"]:
            result_msg = (
                "Available Commands:\n"
                "  help                 - Show this help message\n"
                "  version              - Show firmware & kernel version\n"
                "  status               - Show complete system status\n"
                "  wifi status          - Query WiFi connection state\n"
                "  wifi scan            - Scan for available WiFi networks\n"
                "  wifi connect <s <p>  - Connect to WiFi AP\n"
                "  ble status           - Query NimBLE status\n"
                "  ble start            - Start BLE advertising\n"
                "  ble stop             - Stop BLE\n"
                "  time status          - Show SNTP time status\n"
                "  time sync            - Trigger NTP sync\n"
                "  display test         - Test GC9A01 LCD pattern\n"
                "  display brightness <n>- Set brightness (0-100)\n"
                "  self-test, self_test - Run diagnostic self-test suite\n"
                "  kernel status        - Microkernel execution metrics\n"
                "  kernel tasks         - Microkernel task registry\n"
                "  kernel stats         - JSON kernel statistics\n"
                "  reboot               - Soft reboot the system\n"
                "  factory_reset        - Reset NVS configuration\n"
            )
        elif cmd == "version":
            result_msg = (
                f"Product       SmartDisplay\n"
                f"Model         {MODEL}\n"
                f"Firmware      {FW_VERSION}\n"
                f"Nexos-RT      {KERNEL_VERSION}\n"
                f"Hardware      {HW_VERSION}\n"
                f"Build         {BUILD_TIME}\n"
                f"ESP-IDF       6.1 (Option A Architecture)\n"
            )
        elif cmd in ["status", "system_status", "system_info"]:
            t_m, t_s, ampm, d_str = self.state.get_time_strings()
            result_msg = (
                f"SYSTEM STATUS\n"
                f"------------------------\n"
                f"State          READY\n"
                f"Uptime         {self.state.uptime_sec // 3600:02d}:{(self.state.uptime_sec % 3600) // 60:02d}:{self.state.uptime_sec % 60:02d}\n"
                f"Heap           {self.state.free_heap // 1024} KB\n"
                f"Min Heap       {self.state.min_free_heap // 1024} KB\n"
                f"DISPLAY        GC9A01 240x240 OK\n"
                f"WIFI           {'CONNECTED' if self.state.wifi_connected else 'DISCONNECTED'}\n"
                f"  SSID         {self.state.wifi_ssid or 'N/A'}\n"
                f"  IP           {self.state.wifi_ip or 'N/A'}\n"
                f"  RSSI         {self.state.wifi_rssi} dBm\n"
                f"BLE            {'CONNECTED' if self.state.ble_connected else ('ADVERTISING' if self.state.ble_advertising else 'IDLE')}\n"
                f"TIME           {'SYNCED' if self.state.time_synced else 'NOT SYNCED'} ({self.state.timezone})\n"
                f"  Local        {t_m}{t_s} {ampm} ({d_str})\n"
            )
        elif cmd == "wifi_status":
            result_msg = (
                f"[WIFI]\n"
                f"state : {'CONNECTED' if self.state.wifi_connected else 'DISCONNECTED'}\n"
                f"ssid  : {self.state.wifi_ssid}\n"
                f"rssi  : {self.state.wifi_rssi} dBm\n"
                f"ip    : {self.state.wifi_ip}\n"
            )
        elif cmd == "wifi_scan":
            result_msg = (
                "Scan results:\n"
                "  Home_IoT_5G     (-52 dBm) ch 6  [WPA2]\n"
                "  SmartMesh_Lab   (-64 dBm) ch 1  [WPA2]\n"
                "  Office_Guest    (-78 dBm) ch 11 [OPEN]\n"
            )
        elif cmd == "wifi_connect":
            if len(args) < 1:
                result_status = "FAILED"
                result_msg = "Usage: wifi connect <ssid> [password]"
            else:
                self.state.wifi_ssid = args[0]
                self.state.wifi_connected = True
                self.state.wifi_ip = f"192.168.1.{random.randint(100, 220)}"
                self.state.wifi_rssi = random.randint(-58, -45)
                self.state.time_synced = True
                result_msg = f"Connected to {args[0]} IP={self.state.wifi_ip} RSSI={self.state.wifi_rssi} dBm"
        elif cmd == "ble_status":
            st = "CONNECTED" if self.state.ble_connected else ("ADVERTISING" if self.state.ble_advertising else "IDLE")
            result_msg = f"BLE state: {st} advertising={1 if self.state.ble_advertising else 0} name={self.state.ble_name}"
        elif cmd == "ble_start":
            self.state.ble_advertising = True
            result_msg = "BLE advertising started"
        elif cmd == "ble_stop":
            self.state.ble_advertising = False
            self.state.ble_connected = False
            result_msg = "BLE advertising stopped"
        elif cmd == "time_status":
            t_m, t_s, ampm, d_str = self.state.get_time_strings()
            result_msg = f"Time synced={1 if self.state.time_synced else 0} source=SNTP tz={self.state.timezone} {t_m}{t_s} {ampm} {d_str}"
        elif cmd == "time_sync":
            self.state.time_synced = True
            result_msg = "SNTP synchronized with pool.ntp.org (IST +5:30)"
        elif cmd == "display_test":
            result_msg = "Display test pattern OK: GC9A01 240x240 RGB565 verified"
        elif cmd == "display_brightness":
            if not args:
                result_status = "FAILED"
                result_msg = "Usage: display brightness <0-100>"
            else:
                try:
                    val = int(args[0])
                    if 0 <= val <= 100:
                        self.state.brightness = val
                        result_msg = f"Brightness set to {val}%"
                    else:
                        result_status = "FAILED"
                        result_msg = "Range must be 0-100"
                except ValueError:
                    result_status = "FAILED"
                    result_msg = "Invalid integer"
        elif cmd in ["self_test", "self-test"]:
            tests = [
                ("NVS Storage", True),
                ("Board Config", True),
                ("SPI2 Master", True),
                ("GC9A01 Display", True),
                ("I2C Bus Exp", True),
                ("WiFi Stack", True),
                ("NimBLE Stack", True),
                ("SNTP Time", self.state.time_synced),
                ("Free Heap > 50K", self.state.free_heap > 50000),
            ]
            passed = sum(1 for _, ok in tests if ok)
            total = len(tests)
            msg = "DIAGNOSTIC SELF TEST\n"
            for name, ok in tests:
                msg += f"  [{'PASS' if ok else 'WARN'}] {name}\n"
            msg += f"\nResult: {passed}/{total} Passed - {'ALL OK' if passed==total else 'WARNINGS'}"
            result_msg = msg
        elif cmd == "kernel_status":
            result_msg = (
                f"Nexos-RT      v{KERNEL_VERSION}\n"
                f"Status        RUNNING\n"
                f"Uptime        {self.state.uptime_sec * 1000} ms\n"
                f"Tasks Active  {self.state.active_tasks}\n"
                f"Ctx Switches  {self.state.context_switches}\n"
                f"Free Heap     {self.state.free_heap // 1024} KB\n"
                f"Min Heap      {self.state.min_free_heap // 1024} KB\n"
            )
        elif cmd == "kernel_tasks":
            result_msg = (
                "Nexos-RT Tasks (Active: 4, Max: 16):\n"
                "  ID  NAME        PRIO  STATE    AFFINITY  STACK_FREE\n"
                "  1   GUI         7     READY    Core 1    3120 B\n"
                "  2   COMMAND     6     BLOCKED  Core 1    2840 B\n"
                "  3   CONNECT     5     SLEEP    Core 0    3310 B\n"
                "  4   SYSTEM      3     READY    Core 1    2450 B\n"
            )
        elif cmd == "kernel_stats":
            stats = {
                "version": KERNEL_VERSION,
                "running": 1,
                "uptime_ms": self.state.uptime_sec * 1000,
                "tasks": self.state.active_tasks,
                "ctx_sw": self.state.context_switches,
                "free_heap": self.state.free_heap,
            }
            result_msg = json.dumps(stats, indent=2)
        elif cmd == "reboot":
            self.state.boot_time = time.time()
            self.state.uptime_sec = 0
            self.state.context_switches = 0
            result_msg = "Rebooting system... OK"
        elif cmd in ["factory_reset", "factoryreset"]:
            self.state.wifi_ssid = ""
            self.state.wifi_ip = ""
            self.state.wifi_connected = False
            self.state.time_synced = False
            self.state.brightness = 80
            result_msg = "Factory reset completed: NVS restored to factory defaults"
        else:
            result_status = "FAILED"
            result_msg = f"Unknown command: '{cmd}'. Type 'help' for available commands."

        elapsed_ms = max(1, int((time.time() - t0) * 1000))
        self.state.latest_command = line.upper()
        self.state.command_status = result_status
        if result_status == "SUCCESS":
            self.state.command_message = f"✓ PASS • {result_msg.splitlines()[0] if result_msg else 'OK'}"
        else:
            self.state.command_message = f"✕ FAIL • {result_msg.splitlines()[0]}"
        self.state.command_time_ms = elapsed_ms
        self.state.update_ticks()

        return {
            "status": result_status,
            "message": result_msg,
            "time_ms": elapsed_ms,
        }

def run_cli_mode(state: DeviceState, engine: CommandEngine):
    print("=" * 60)
    print(" Smart Device Firmware - CLI Simulator")
    print(f" FW v{FW_VERSION} | Kernel v{KERNEL_VERSION} | Target: ESP32-S3")
    print(" Type 'help' for command list. Type 'exit' to quit.")
    print("=" * 60)
    
    while True:
        try:
            line = input("device> ").strip()
            if line.lower() in ["exit", "quit"]:
                break
            if not line:
                continue
            res = engine.execute(line)
            print(res["message"])
            print(f"[{res['status']}] in {res['time_ms']} ms\n")
        except (KeyboardInterrupt, EOFError):
            print("\nExiting simulator.")
            break

def run_gui_mode(state: DeviceState, engine: CommandEngine):
    try:
        import tkinter as tk
        from tkinter import ttk, messagebox
    except ImportError:
        print("[WARN] Tkinter not available, falling back to CLI mode.")
        run_cli_mode(state, engine)
        return

    root = tk.Tk()
    root.title(f"Smart Device Simulator - GC9A01 240x240 Round Display (FW v{FW_VERSION})")
    root.geometry("860x560")
    root.configure(bg="#0B0D13")

    # Left Frame: Circular Display Simulation
    disp_frame = tk.Frame(root, bg="#0B0D13", padx=20, pady=20)
    disp_frame.pack(side=tk.LEFT, fill=tk.BOTH)

    canvas_size = 280
    canvas = tk.Canvas(disp_frame, width=canvas_size, height=canvas_size, bg="#0B0D13", highlightthickness=0)
    canvas.pack(pady=10)

    # Right Frame: Interactive Shell & Controls
    control_frame = tk.Frame(root, bg="#12151E", padx=15, pady=15)
    control_frame.pack(side=tk.RIGHT, fill=tk.BOTH, expand=True)

    title_lbl = tk.Label(control_frame, text="ESP32-S3 Serial Command Console", font=("Consolas", 12, "bold"), fg="#00D1FF", bg="#12151E")
    title_lbl.pack(anchor="w", pady=(0, 5))

    # Console output
    console_txt = tk.Text(control_frame, height=16, bg="#08090C", fg="#00FF88", font=("Consolas", 10), insertbackground="#00D1FF", relief=tk.FLAT)
    console_txt.pack(fill=tk.BOTH, expand=True, pady=5)
    console_txt.insert(tk.END, f"================================================\n Smart Device Firmware - Simulator Ready\n FW {FW_VERSION} | Kernel {KERNEL_VERSION} | GC9A01 240x240\n Type 'help' for command list\n================================================\n\n")

    # Entry frame
    entry_frame = tk.Frame(control_frame, bg="#12151E")
    entry_frame.pack(fill=tk.X, pady=5)

    prompt_lbl = tk.Label(entry_frame, text="device> ", font=("Consolas", 11, "bold"), fg="#00D1FF", bg="#12151E")
    prompt_lbl.pack(side=tk.LEFT)

    cmd_entry = tk.Entry(entry_frame, font=("Consolas", 11), bg="#1B1F2B", fg="#FFFFFF", insertbackground="#00D1FF", relief=tk.FLAT)
    cmd_entry.pack(side=tk.LEFT, fill=tk.X, expand=True, padx=5)

    # Quick button presets
    btn_frame = tk.Frame(control_frame, bg="#12151E")
    btn_frame.pack(fill=tk.X, pady=5)

    def execute_cmd(cmd_text: str):
        console_txt.insert(tk.END, f"device> {cmd_text}\n")
        res = engine.execute(cmd_text)
        console_txt.insert(tk.END, f"{res['message']}\n")
        console_txt.insert(tk.END, f"[{res['status']}] {res['time_ms']} ms\n\n")
        console_txt.see(tk.END)

    def on_enter(event=None):
        cmd = cmd_entry.get().strip()
        if cmd:
            cmd_entry.delete(0, tk.END)
            execute_cmd(cmd)

    cmd_entry.bind("<Return>", on_enter)

    def add_btn(text: str, command_str: str):
        b = tk.Button(btn_frame, text=text, command=lambda: execute_cmd(command_str), bg="#1F2432", fg="#E0E6ED", font=("Segoe UI", 8, "bold"), relief=tk.FLAT, padx=6, pady=3, activebackground="#00D1FF", activeforeground="#000000")
        b.pack(side=tk.LEFT, padx=3)

    add_btn("Help", "help")
    add_btn("Version", "version")
    add_btn("Status", "status")
    add_btn("WiFi Scan", "wifi scan")
    add_btn("WiFi Connect", "wifi connect Home_IoT_5G pass123")
    add_btn("Self Test", "self-test")
    add_btn("Kernel", "kernel status")
    add_btn("Reboot", "reboot")

    # Rendering function for Circular 240x240 LCD Glassmorphic UI
    def render_lcd():
        canvas.delete("all")
        cx, cy = canvas_size // 2, canvas_size // 2
        r = 120  # 240 diameter
        
        # Outer bezel
        canvas.create_oval(cx - r - 12, cy - r - 12, cx + r + 12, cy + r + 12, fill="#1B1F2A", outline="#2A3142", width=2)
        canvas.create_oval(cx - r - 4, cy - r - 4, cx + r + 4, cy + r + 4, fill="#0A0A0F", outline="#00D1FF", width=1)
        
        # Screen Base Circle (240x240)
        canvas.create_oval(cx - r, cy - r, cx + r, cy + r, fill="#0F1118", outline="")
        
        # Outer Glow Arc (270 deg open ring)
        arc_r = 114
        canvas.create_arc(cx - arc_r, cy - arc_r, cx + arc_r, cy + arc_r, start=45, extent=270, style=tk.ARC, outline="#00D1FF", width=2)
        
        # Inner Ring
        in_r = 106
        canvas.create_oval(cx - in_r, cy - in_r, cx + in_r, cy + in_r, outline="#222838", width=1)
        
        # Top Status Pills
        # WiFi Pill
        wifi_color = "#00FF88" if state.wifi_connected else "#FF3B30"
        canvas.create_rectangle(cx - 85, cy - 95, cx - 25, cy - 75, fill="#1A1D28", outline="#2A3142", width=1)
        canvas.create_oval(cx - 78, cy - 88, cx - 72, cy - 82, fill=wifi_color, outline="")
        wifi_txt = f"WiFi {state.wifi_rssi}" if state.wifi_connected else "WiFi --"
        canvas.create_text(cx - 50, cy - 85, text=wifi_txt, fill="#D1D5DB", font=("Consolas", 7, "bold"))
        
        # BLE Pill
        ble_color = "#00FF88" if state.ble_connected else ("#FF9F0A" if state.ble_advertising else "#FF3B30")
        canvas.create_rectangle(cx + 25, cy - 95, cx + 85, cy - 75, fill="#1A1D28", outline="#2A3142", width=1)
        canvas.create_oval(cx + 32, cy - 88, cx + 38, cy - 82, fill=ble_color, outline="")
        canvas.create_text(cx + 58, cy - 85, text="BLE", fill="#D1D5DB", font=("Consolas", 7, "bold"))
        
        # Time Cluster
        t_main, t_sec, ampm, d_str = state.get_time_strings()
        canvas.create_text(cx - 14, cy - 45, text=t_main, fill="#FFFFFF", font=("Segoe UI", 26, "bold"))
        canvas.create_text(cx + 42, cy - 43, text=t_sec, fill="#8A8F98", font=("Segoe UI", 12, "bold"))
        if ampm:
            canvas.create_text(cx + 42, cy - 58, text=ampm, fill="#00D1FF", font=("Segoe UI", 7, "bold"))
            
        canvas.create_text(cx, cy - 20, text=d_str, fill="#00D1FF", font=("Segoe UI", 9, "bold"))
        
        # Divider line
        canvas.create_line(cx - 40, cy - 8, cx + 40, cy - 8, fill="#00D1FF", width=1)
        
        # System Status Chip
        chip_bg = "#0F2E1F" if "OK" in state.system_status else ("#2E2410" if "SYNC" in state.system_status else "#2E1515")
        chip_fg = "#00FF88" if "OK" in state.system_status else ("#FF9F0A" if "SYNC" in state.system_status else "#FF3B30")
        canvas.create_rectangle(cx - 50, cy + 2, cx + 50, cy + 22, fill=chip_bg, outline=chip_fg, width=1)
        canvas.create_oval(cx - 42, cy + 9, cx - 36, cy + 15, fill=chip_fg, outline="")
        canvas.create_text(cx + 3, cy + 12, text=state.system_status, fill=chip_fg, font=("Segoe UI", 8, "bold"))
        
        # Command Card
        card_border = "#00FF88" if state.command_status == "SUCCESS" else "#FF3B30"
        canvas.create_rectangle(cx - 85, cy + 30, cx + 85, cy + 72, fill="#161922", outline=card_border, width=1)
        
        cmd_short = state.latest_command[:22]
        canvas.create_text(cx - 80, cy + 40, text=f"CMD: {cmd_short}", fill="#9CA3AF", anchor="w", font=("Consolas", 7, "bold"))
        canvas.create_text(cx + 80, cy + 40, text=f"{state.command_time_ms}ms", fill="#6B7280", anchor="e", font=("Consolas", 7))
        
        msg_short = state.command_message[:24]
        msg_color = "#00FF88" if state.command_status == "SUCCESS" else "#FF3B30"
        canvas.create_text(cx - 80, cy + 58, text=msg_short, fill=msg_color, anchor="w", font=("Segoe UI", 8, "bold"))
        
        # Bottom Info
        canvas.create_text(cx - 55, cy + 92, text=f"FW v{FW_VERSION}", fill="#6B7280", font=("Segoe UI", 7))
        uptime_str = f"{state.uptime_sec // 3600:02d}:{(state.uptime_sec % 3600) // 60:02d}:{state.uptime_sec % 60:02d}"
        canvas.create_text(cx + 55, cy + 92, text=uptime_str, fill="#6B7280", font=("Segoe UI", 7))
        
        # Tiny Heap Bar
        canvas.create_rectangle(cx - 20, cy + 102, cx + 20, cy + 104, fill="#1E222E", outline="")
        canvas.create_rectangle(cx - 20, cy + 102, cx + 8, cy + 104, fill="#00D1FF", outline="")

    # Update loop (every 100ms)
    def update_loop():
        state.update_ticks()
        render_lcd()
        root.after(100, update_loop)

    update_loop()
    root.mainloop()

def main():
    state = DeviceState()
    engine = CommandEngine(state)
    
    if "--cli" in sys.argv:
        run_cli_mode(state, engine)
    else:
        run_gui_mode(state, engine)

if __name__ == "__main__":
    main()

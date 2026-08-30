#!/usr/bin/env python3
"""
Smart Device Platform - Web Simulator Server
Serves interactive glassmorphism UI & REST API for command execution.
"""

import os
import sys
import json
import socket
from http.server import HTTPServer, SimpleHTTPRequestHandler
from pathlib import Path

# Add simulator folder
sys.path.insert(0, str(Path(__file__).parent))
from run_simulator import DeviceState, CommandEngine

PORT = 8080
WEB_DIR = Path(__file__).parent / "web"

state = DeviceState()
engine = CommandEngine(state)

class SimulatorHTTPHandler(SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=str(WEB_DIR), **kwargs)

    def do_POST(self):
        if self.path == "/api/command":
            content_len = int(self.headers.get("Content-Length", 0))
            post_body = self.rfile.read(content_len).decode("utf-8")
            try:
                data = json.loads(post_body)
                cmd = data.get("command", "")
                result = engine.execute(cmd)
                
                self.send_response(200)
                self.send_header("Content-Type", "application/json")
                self.send_header("Access-Control-Allow-Origin", "*")
                self.end_headers()
                self.wfile.write(json.dumps(result).encode("utf-8"))
            except Exception as e:
                self.send_response(400)
                self.end_headers()
                self.wfile.write(json.dumps({"error": str(e)}).encode("utf-8"))
        else:
            self.send_error(404, "Endpoint not found")

    def do_GET(self):
        if self.path == "/api/state":
            state.update_ticks()
            t_m, t_s, ampm, d_str = state.get_time_strings()
            payload = {
                "wifi_connected": state.wifi_connected,
                "wifi_ssid": state.wifi_ssid,
                "wifi_ip": state.wifi_ip,
                "wifi_rssi": state.wifi_rssi,
                "ble_advertising": state.ble_advertising,
                "ble_connected": state.ble_connected,
                "time_synced": state.time_synced,
                "timezone": state.timezone,
                "time_str": f"{t_m}{t_s} {ampm}",
                "date_str": d_str,
                "brightness": state.brightness,
                "system_status": state.system_status,
                "latest_command": state.latest_command,
                "command_status": state.command_status,
                "command_message": state.command_message,
                "uptime_sec": state.uptime_sec,
                "free_heap": state.free_heap,
                "min_free_heap": state.min_free_heap,
                "context_switches": state.context_switches,
            }
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Access-Control-Allow-Origin", "*")
            self.end_headers()
            self.wfile.write(json.dumps(payload).encode("utf-8"))
        else:
            super().do_GET()

def run_server(port=PORT):
    # Enable address reuse
    server_address = ("", port)
    httpd = HTTPServer(server_address, SimulatorHTTPHandler)
    print(f"==================================================")
    print(f" Smart Device Web Simulator Running!")
    print(f" Access URL: http://localhost:{port}")
    print(f"==================================================")
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\nShutting down server.")
        httpd.server_close()

if __name__ == "__main__":
    p = PORT
    if len(sys.argv) > 1 and sys.argv[1].isdigit():
        p = int(sys.argv[1])
    run_server(p)

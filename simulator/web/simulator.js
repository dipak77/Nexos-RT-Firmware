// Smart Device Platform - Interactive Simulator Client
let deviceState = {
    wifi_connected: false,
    wifi_ssid: "",
    wifi_ip: "",
    wifi_rssi: -65,
    ble_enabled: true,
    ble_advertising: true,
    ble_connected: false,
    time_synced: false,
    timezone: "IST-5:30",
    brightness: 80,
    system_status: "NO WIFI",
    latest_command: "READY",
    command_status: "SUCCESS",
    command_message: "SYSTEM READY",
    command_time_ms: 4,
    uptime_sec: 0,
    free_heap: 320 * 1024,
    min_free_heap: 280 * 1024,
    context_switches: 1420,
    boot_time: Date.now()
};

let cmdHistory = [];
let historyIndex = -1;

function updateClock() {
    const now = new Date();
    
    // Format 12-hour time
    let hours = now.getHours();
    const ampm = hours >= 12 ? 'PM' : 'AM';
    hours = hours % 12;
    hours = hours ? hours : 12; // 0 becomes 12
    const minutes = String(now.getMinutes()).padStart(2, '0');
    const seconds = String(now.getSeconds()).padStart(2, '0');
    
    // Date: 28 AUG 2026
    const months = ["JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"];
    const dateStr = `${String(now.getDate()).padStart(2, '0')} ${months[now.getMonth()]} ${now.getFullYear()}`;

    // Update Display DOM
    const timeMainEl = document.getElementById('time-main');
    const timeSecEl = document.getElementById('time-sec');
    const timeAmpmEl = document.getElementById('time-ampm');
    const dateStrEl = document.getElementById('date-str');

    if (timeMainEl) timeMainEl.textContent = `${hours}:${minutes}`;
    if (timeSecEl) timeSecEl.textContent = `:${seconds}`;
    if (timeAmpmEl) timeAmpmEl.textContent = ampm;
    if (dateStrEl) dateStrEl.textContent = dateStr;

    // Update Uptime
    deviceState.uptime_sec = Math.floor((Date.now() - deviceState.boot_time) / 1000);
    const hrs = String(Math.floor(deviceState.uptime_sec / 3600)).padStart(2, '0');
    const mins = String(Math.floor((deviceState.uptime_sec % 3600) / 60)).padStart(2, '0');
    const secs = String(deviceState.uptime_sec % 60).padStart(2, '0');
    const uptimeFormatted = `${hrs}:${mins}:${secs}`;

    const uptimeTag = document.getElementById('uptime-tag');
    const chipUptime = document.getElementById('chip-uptime');
    if (uptimeTag) uptimeTag.textContent = uptimeFormatted;
    if (chipUptime) chipUptime.textContent = `UPTIME: ${uptimeFormatted}`;

    // Context Switches increment
    deviceState.context_switches += Math.floor(Math.random() * 3) + 1;
    const statCtx = document.getElementById('stat-ctx-sw');
    if (statCtx) statCtx.textContent = deviceState.context_switches.toLocaleString();
}

function updateUiElements() {
    // WiFi Pill
    const dotWifi = document.getElementById('dot-wifi');
    const lblWifi = document.getElementById('lbl-wifi');
    if (dotWifi && lblWifi) {
        if (deviceState.wifi_connected) {
            dotWifi.classList.add('active');
            lblWifi.textContent = `WiFi ${deviceState.wifi_rssi}`;
        } else {
            dotWifi.classList.remove('active');
            lblWifi.textContent = `WiFi --`;
        }
    }

    // BLE Pill
    const dotBle = document.getElementById('dot-ble');
    const lblBle = document.getElementById('lbl-ble');
    if (dotBle && lblBle) {
        if (deviceState.ble_connected) {
            dotBle.style.backgroundColor = '#00FF88';
            lblBle.textContent = 'BLE •';
        } else if (deviceState.ble_advertising) {
            dotBle.style.backgroundColor = '#FF9F0A';
            lblBle.textContent = 'BLE';
        } else {
            dotBle.style.backgroundColor = '#FF3B30';
            lblBle.textContent = 'BLE';
        }
    }

    // System Health Status Chip
    const statusChip = document.getElementById('status-chip');
    const chipDot = document.getElementById('chip-dot');
    const chipText = document.getElementById('chip-text');
    if (statusChip && chipDot && chipText) {
        if (deviceState.wifi_connected && deviceState.time_synced) {
            deviceState.system_status = "SYSTEM OK";
            statusChip.className = "status-chip chip-ok";
            chipDot.style.backgroundColor = "#00FF88";
            chipText.style.color = "#00FF88";
            chipText.textContent = "SYSTEM OK";
        } else if (deviceState.wifi_connected) {
            deviceState.system_status = "SYNCING TIME";
            statusChip.className = "status-chip chip-warn";
            chipDot.style.backgroundColor = "#FF9F0A";
            chipText.style.color = "#FF9F0A";
            chipText.textContent = "SYNCING TIME";
        } else {
            deviceState.system_status = "NO WIFI";
            statusChip.className = "status-chip chip-err";
            chipDot.style.backgroundColor = "#FF3B30";
            chipText.style.color = "#FF3B30";
            chipText.textContent = "NO WIFI";
        }
    }

    // Command Card
    const cmdCard = document.getElementById('cmd-card');
    const cmdTitle = document.getElementById('cmd-card-title');
    const cmdTime = document.getElementById('cmd-card-time');
    const cmdIcon = document.getElementById('cmd-card-icon');
    const cmdMsg = document.getElementById('cmd-card-msg');

    if (cmdCard && cmdTitle && cmdMsg) {
        cmdTitle.textContent = `CMD: ${deviceState.latest_command.toUpperCase()}`;
        cmdTime.textContent = `${deviceState.command_time_ms} ms`;

        if (deviceState.command_status === "SUCCESS") {
            cmdCard.className = "cmd-glass-card pass";
            cmdIcon.textContent = "✓";
            cmdIcon.style.color = "#00FF88";
            cmdMsg.textContent = deviceState.command_message;
        } else {
            cmdCard.className = "cmd-glass-card fail";
            cmdIcon.textContent = "✕";
            cmdIcon.style.color = "#FF3B30";
            cmdMsg.textContent = deviceState.command_message;
        }
    }

    // Heap metrics
    const chipHeap = document.getElementById('chip-heap');
    const statHeap = document.getElementById('stat-free-heap');
    if (chipHeap) chipHeap.textContent = `HEAP: ${Math.round(deviceState.free_heap / 1024)} KB`;
    if (statHeap) statHeap.textContent = `${Math.round(deviceState.free_heap / 1024)} KB`;
}

function appendTerminal(text, type = "normal") {
    const term = document.getElementById('terminal-screen');
    if (!term) return;

    const div = document.createElement('div');
    div.className = `term-line ${type}`;
    div.textContent = text;
    term.appendChild(div);
    term.scrollTop = term.scrollHeight;
}

async function sendCommand(cmd) {
    if (!cmd || !cmd.trim()) return;
    cmd = cmd.trim();

    cmdHistory.push(cmd);
    historyIndex = cmdHistory.length;

    appendTerminal(`device> ${cmd}`, 'term-cmd');

    // Call backend API if available, else local execution
    try {
        const response = await fetch('/api/command', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ command: cmd })
        });
        if (response.ok) {
            const data = await response.json();
            handleCommandResponse(cmd, data);
            return;
        }
    } catch (e) {
        // Fallback to client-side engine
    }

    // Client-side execution fallback
    executeLocalCommand(cmd);
}

function executeLocalCommand(cmd) {
    const t0 = performance.now();
    const tokens = cmd.split(/\s+/);
    let c = tokens[0].toLowerCase();
    let args = tokens.slice(1);

    if (tokens.length >= 2) {
        const compound = `${tokens[0]}_${tokens[1]}`.toLowerCase();
        if (["wifi_status", "wifi_scan", "wifi_connect", "ble_status", "ble_start", "ble_stop",
             "time_status", "time_sync", "display_test", "display_brightness", "self_test",
             "kernel_status", "kernel_tasks", "kernel_stats", "system_info", "system_status",
             "factory_reset"].includes(compound)) {
            c = compound;
            args = tokens.slice(2);
        }
    }

    let status = "SUCCESS";
    let msg = "";

    switch(c) {
        case "switch_kernel":
        case "switch_kernel_freertos":
        case "switch_kernel_microkernel":
            if (args.length > 0 && args[0].toLowerCase().includes("micro")) {
                deviceState.active_kernel = "Custom Microkernel (Secondary)";
                msg = "========================================================\n [KERNEL SWITCH] Active Kernel -> Custom Microkernel\n [KERNEL SWITCH] Running Microkernel Cooperative Engine\n========================================================";
            } else {
                deviceState.active_kernel = "FreeRTOS SMP (Primary)";
                msg = "========================================================\n [KERNEL SWITCH] Active Kernel -> FreeRTOS SMP (Primary)\n [KERNEL SWITCH] Full Dual-Core Preemption Restored\n========================================================";
            }
            break;
        case "kernel_info":
            msg = `================ KERNEL SUBSYSTEM INFO ================\n Active Kernel       : ${deviceState.active_kernel || 'FreeRTOS SMP (Primary)'}\n Kernel Version      : FreeRTOS v10.5.1-esp32s3 / Microkernel v1.0.0\n Execution Model     : Dual-Core Preemptive Multi-threading\n CPU Cores Allocated : 2 Core(s) (Xtensa LX7 @ 240MHz)\n Active Tasks/Threads: 9\n Free Internal SRAM  : 346 KB (Min: 346 KB)\n Free PSRAM (8MB)    : 8192 KB\n System Uptime       : ${deviceState.uptime_sec} seconds\n=======================================================`;
            break;
        case "tasks":
            msg = `--- FREERTOS TASK TABLE ---\n GUI Task        : Handle=0x3fceeab8 | Core=1 | Priority=5 | State=RUNNING\n SysMon Task     : Handle=0x3fceec20 | Core=0 | Priority=2 | State=RUNNING\n CLI Task        : Handle=0x3fcf41d8 | Core=0 | Priority=3 | State=RUNNING\n Total Tasks     : 9`;
            break;
        case "help":
        case "?":
            msg = `Available commands:\n  help, status, tasks, kernel_info, switch_kernel <freertos|microkernel>,\n  wifi scan, wifi connect <ssid> <pass>, wifi status, ble status, time sync,\n  display test, reboot`;
            break;
        case "version":
            msg = `Product       SmartDisplay\nModel         S3-GC9A01\nFirmware      1.0.0\nKernel        0.8.0 (Microkernel Edition)\nHardware      S3-DK-V1.1\nIDF Target    ESP-IDF 6.1 (Option A Dual-Core)`;
            break;
        case "status":
        case "system_status":
        case "system_info":
            msg = `SYSTEM STATUS\nState: READY | Heap: ${Math.round(deviceState.free_heap/1024)}KB | WiFi: ${deviceState.wifi_connected?'CONNECTED':'DISCONNECTED'} (${deviceState.wifi_ssid || 'N/A'})\nBLE: ${deviceState.ble_advertising?'ADVERTISING':'IDLE'} | SNTP: ${deviceState.time_synced?'SYNCED':'NOT SYNCED'} (${deviceState.timezone})\nDisplay: GC9A01 240x240 RGB565 OK`;
            break;
        case "wifi_scan":
            msg = `Scan results:\n  Home_IoT_5G     (-52 dBm) ch 6  [WPA2]\n  SmartMesh_Lab   (-64 dBm) ch 1  [WPA2]\n  Office_Guest    (-78 dBm) ch 11 [OPEN]`;
            break;
        case "wifi_connect":
            if (args.length < 1) {
                status = "FAILED";
                msg = "Usage: wifi connect <ssid> [password]";
            } else {
                deviceState.wifi_ssid = args[0];
                deviceState.wifi_connected = true;
                deviceState.wifi_ip = `192.168.1.${Math.floor(Math.random()*120)+100}`;
                deviceState.wifi_rssi = -54;
                deviceState.time_synced = true;
                msg = `Connected to ${args[0]} IP=${deviceState.wifi_ip} RSSI=${deviceState.wifi_rssi} dBm`;
            }
            break;
        case "wifi_status":
            msg = `[WIFI]\nstate : ${deviceState.wifi_connected?'CONNECTED':'DISCONNECTED'}\nssid  : ${deviceState.wifi_ssid || 'None'}\nrssi  : ${deviceState.wifi_rssi} dBm\nip    : ${deviceState.wifi_ip || '0.0.0.0'}`;
            break;
        case "ble_status":
            msg = `BLE state: ${deviceState.ble_connected?'CONNECTED':(deviceState.ble_advertising?'ADVERTISING':'IDLE')} advertising=${deviceState.ble_advertising?1:0} name=SmartDisplay-BLE`;
            break;
        case "ble_start":
            deviceState.ble_advertising = true;
            msg = "BLE advertising started (SmartDisplay-BLE)";
            break;
        case "ble_stop":
            deviceState.ble_advertising = false;
            msg = "BLE advertising stopped";
            break;
        case "time_status":
            msg = `Time synced=${deviceState.time_synced?1:0} source=SNTP tz=${deviceState.timezone}`;
            break;
        case "time_sync":
            deviceState.time_synced = true;
            msg = "SNTP synchronized with pool.ntp.org (IST +5:30)";
            break;
        case "display_test":
            msg = "GC9A01 LCD test pattern executed: RGB565 color inversion OK";
            break;
        case "display_brightness":
            if (args.length > 0 && !isNaN(parseInt(args[0]))) {
                const b = Math.min(100, Math.max(0, parseInt(args[0])));
                deviceState.brightness = b;
                updateBrightness(b);
                msg = `Brightness set to ${b}%`;
            } else {
                status = "FAILED";
                msg = "Usage: display brightness <0-100>";
            }
            break;
        case "self_test":
        case "self-test":
            msg = `SELF TEST\n  [PASS] NVS Storage\n  [PASS] Board Config S3-DK-V1.1\n  [PASS] SPI2 Master MOSI=11 CLK=12\n  [PASS] GC9A01 Display 240x240\n  [PASS] I2C Bus Exp SDA=5 SCL=6\n  [PASS] WiFi Stack\n  [PASS] NimBLE Stack\n  [PASS] SNTP Time\n  [PASS] Free Heap Check (>50KB)\n10/10 TESTS PASSED - RESULT: PASS`;
            break;
        case "kernel_status":
            msg = `Microkernel   v0.8.0\nStatus        RUNNING\nUptime        ${deviceState.uptime_sec*1000} ms\nTasks Active  4\nCtx Switches  ${deviceState.context_switches}\nFree Heap     ${Math.round(deviceState.free_heap/1024)} KB`;
            break;
        case "kernel_tasks":
            msg = `Microkernel Tasks (Active: 4, Max: 16):\n  ID  NAME        PRIO  STATE    AFFINITY  STACK_FREE\n  1   GUI         7     READY    Core 1    3120 B\n  2   COMMAND     6     BLOCKED  Core 1    2840 B\n  3   CONNECT     5     SLEEP    Core 0    3310 B\n  4   SYSTEM      3     READY    Core 1    2450 B`;
            break;
        case "kernel_stats":
            msg = JSON.stringify({
                version: "0.8.0",
                running: 1,
                uptime_ms: deviceState.uptime_sec * 1000,
                tasks: 4,
                ctx_sw: deviceState.context_switches,
                free_heap: deviceState.free_heap
            }, null, 2);
            break;
        case "reboot":
            deviceState.boot_time = Date.now();
            deviceState.uptime_sec = 0;
            deviceState.context_switches = 0;
            msg = "Rebooting system... OK (Microkernel v0.8.0 restarted)";
            break;
        case "factory_reset":
            deviceState.wifi_ssid = "";
            deviceState.wifi_ip = "";
            deviceState.wifi_connected = false;
            deviceState.time_synced = false;
            deviceState.brightness = 80;
            updateBrightness(80);
            msg = "Factory reset done: NVS cleared to default configuration";
            break;
        default:
            status = "FAILED";
            msg = `Unknown command: '${cmd}'. Type 'help' for available commands.`;
    }

    const elapsed = Math.max(1, Math.round(performance.now() - t0));
    handleCommandResponse(cmd, {
        status: status,
        message: msg,
        time_ms: elapsed
    });
}

function handleCommandResponse(cmd, data) {
    deviceState.latest_command = cmd;
    deviceState.command_status = data.status;
    deviceState.command_time_ms = data.time_ms || 3;
    deviceState.command_message = data.status === "SUCCESS" 
        ? `PASS • ${data.message.split('\n')[0]}`
        : `FAIL • ${data.message.split('\n')[0]}`;

    appendTerminal(data.message, data.status === 'SUCCESS' ? 'term-out' : 'term-err');
    appendTerminal(`[${data.status}] in ${deviceState.command_time_ms} ms\n`, 'term-banner');

    updateUiElements();
}

function triggerCommand(cmdText) {
    const input = document.getElementById('cmd-input');
    if (input) input.value = cmdText;
    sendCommand(cmdText);
}

function handleCommandSubmit(e) {
    e.preventDefault();
    const input = document.getElementById('cmd-input');
    if (!input) return;
    const cmd = input.value;
    input.value = '';
    sendCommand(cmd);
}

function updateBrightness(val) {
    const screen = document.getElementById('screen-content');
    const label = document.getElementById('brightness-val');
    const slider = document.getElementById('brightness-slider');
    
    if (slider) slider.value = val;
    if (label) label.textContent = `${val}%`;
    if (screen) {
        screen.style.opacity = `${val / 100}`;
    }
}

function clearTerminal() {
    const term = document.getElementById('terminal-screen');
    if (term) term.innerHTML = '';
}

function exportLogs() {
    const term = document.getElementById('terminal-screen');
    if (!term) return;
    const text = term.innerText;
    const blob = new Blob([text], { type: 'text/plain' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `smart_device_logs_${Date.now()}.txt`;
    a.click();
    URL.revokeObjectURL(url);
}

// Key navigation for command history
document.addEventListener('DOMContentLoaded', () => {
    const input = document.getElementById('cmd-input');
    if (input) {
        input.addEventListener('keydown', (e) => {
            if (e.key === 'ArrowUp') {
                if (historyIndex > 0) {
                    historyIndex--;
                    input.value = cmdHistory[historyIndex];
                }
            } else if (e.key === 'ArrowDown') {
                if (historyIndex < cmdHistory.length - 1) {
                    historyIndex++;
                    input.value = cmdHistory[historyIndex];
                } else {
                    historyIndex = cmdHistory.length;
                    input.value = '';
                }
            }
        });
    }

    // Start Real-time loop
    setInterval(updateClock, 1000);
    setInterval(updateUiElements, 500);
    updateClock();
    updateUiElements();
});

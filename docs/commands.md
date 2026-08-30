# Commands

All transports (UART, USB CDC, BLE, WiFi TCP/WS) route to same CommandService.

## List
help
system info / system status
version
wifi status / wifi scan / wifi connect <ssid> <pass> / wifi disconnect
ble status / ble start / ble stop
display test / display brightness 0-100
time status / time sync
ota status / ota update <url>
self-test / self_test
kernel status / kernel tasks / kernel stats
reset-info
reboot / factory reset

`switch_kernel` is not a product command. Next OS is the only OS.

## Result Object
Every command returns CommandResult { id, command_id, status, message, error_code, execution_time_ms }

## GUI Indicator
COMMAND RECEIVED -> WORKING ... -> SUCCESS ✓ PASS / FAIL ✕

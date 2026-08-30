# Smart Device Platform - Real-time USB & COM Port Connection Watcher
[CmdletBinding()]
param(
    [int]$TimeoutSeconds = 60
)

Write-Host "=========================================================" -ForegroundColor Cyan
Write-Host " ESP32-S3 USB & COM Port Live Connection Monitor" -ForegroundColor Cyan
Write-Host " Waiting for ESP32-S3 / CP210x / CH340 / JTAG to connect..." -ForegroundColor Yellow
Write-Host "=========================================================" -ForegroundColor Cyan

$startTime = Get-Date

while (((Get-Date) - $startTime).TotalSeconds -lt $TimeoutSeconds) {
    # Check Win32_SerialPort
    $ports = Get-CimInstance Win32_SerialPort -ErrorAction SilentlyContinue
    if ($ports) {
        Write-Host "`n[SUCCESS] Active COM Port Detected!" -ForegroundColor Green
        foreach ($p in $ports) {
            Write-Host "  Port        : $($p.DeviceID)" -ForegroundColor Cyan
            Write-Host "  Name        : $($p.Name)" -ForegroundColor Green
            Write-Host "  Description : $($p.Description)" -ForegroundColor White
        }
        Write-Host "`nYou can now run: powershell -ExecutionPolicy Bypass -File release/flash_device.ps1 -Port $($ports[0].DeviceID)" -ForegroundColor Yellow
        exit 0
    }

    # Check PnP Devices matching ESP32 / USB UART
    $pnp = Get-PnpDevice -PresentOnly -ErrorAction SilentlyContinue | Where-Object {
        $_.FriendlyName -like '*CP210*' -or
        $_.FriendlyName -like '*CH34*' -or
        $_.FriendlyName -like '*USB Serial*' -or
        $_.FriendlyName -like '*USB JTAG*' -or
        $_.FriendlyName -like '*Espressif*' -or
        $_.InstanceId -match 'VID_10C4' -or
        $_.InstanceId -match 'VID_303A' -or
        $_.InstanceId -match 'VID_1A86'
    }

    if ($pnp) {
        Write-Host "`n[FOUND] USB Device Connected:" -ForegroundColor Green
        $pnp | Format-Table Status, Class, FriendlyName, InstanceId -AutoSize
        exit 0
    }

    Write-Host "." -NoNewline -ForegroundColor Gray
    Start-Sleep -Seconds 1
}

Write-Host "`n[TIMEOUT] No active serial port detected within $TimeoutSeconds seconds." -ForegroundColor Red
Write-Host "Troubleshooting Checklist:" -ForegroundColor Yellow
Write-Host " 1. Ensure your USB-C cable is a DATA cable (not a power-only charge cable)." -ForegroundColor Gray
Write-Host " 2. If your board has 2 USB-C ports ('USB' and 'UART'), try plugging into the other port." -ForegroundColor Gray
Write-Host " 3. Hold down the 'BOOT' button on the board, plug in the USB cable, then release 'BOOT'." -ForegroundColor Gray
Write-Host " 4. Try plugging directly into a PC USB port instead of an unpowered USB hub." -ForegroundColor Gray

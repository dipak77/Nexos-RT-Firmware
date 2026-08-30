# Smart Device Firmware - Flash Tool
[CmdletBinding()]
param(
    [string]$Port,
    [switch]$Monitor
)

if (-not $Port) {
    $foundPorts = Get-CimInstance Win32_SerialPort -ErrorAction SilentlyContinue
    if ($foundPorts) {
        $Port = $foundPorts[0].DeviceID
        Write-Host "Auto-detected port: $Port ($($foundPorts[0].Name))" -ForegroundColor Cyan
    } else {
        $Port = "COM7"
        Write-Host "No active COM port auto-detected, defaulting to $Port" -ForegroundColor Yellow
    }
}

Write-Host "Flashing firmware to ESP32-S3 on $Port..." -ForegroundColor Cyan

if ($Monitor) {
    idf.py -p $Port flash monitor
} else {
    idf.py -p $Port flash
}

if ($LASTEXITCODE -eq 0) {
    Write-Host "[SUCCESS] Flashing completed on $Port" -ForegroundColor Green
} else {
    Write-Host "[FAILED] Flashing failed on $Port" -ForegroundColor Red
}

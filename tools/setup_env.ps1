# Smart Device Firmware - Environment Setup & Diagnostic Tool
[CmdletBinding()]
param(
    [switch]$CheckOnly,
    [switch]$InstallDeps
)

Write-Host "==================================================" -ForegroundColor Cyan
Write-Host " Smart Device Platform - Environment Setup Checker" -ForegroundColor Cyan
Write-Host " Target: ESP32-S3 | GC9A01 240x240 | LVGL 9.5" -ForegroundColor Cyan
Write-Host "==================================================" -ForegroundColor Cyan

# 1. Python Check
Write-Host "`n[1/5] Checking Python..." -NoNewline
try {
    $pyVer = python --version 2>&1
    Write-Host " OK ($pyVer)" -ForegroundColor Green
} catch {
    Write-Host " FAILED (Python not in PATH)" -ForegroundColor Red
}

# 2. Git Check
Write-Host "[2/5] Checking Git..." -NoNewline
try {
    $gitVer = git --version 2>&1
    Write-Host " OK ($gitVer)" -ForegroundColor Green
} catch {
    Write-Host " FAILED" -ForegroundColor Red
}

# 3. ESP-IDF Check
Write-Host "[3/5] Checking ESP-IDF (idf.py)..." -NoNewline
$idfFound = $false
try {
    $idfVer = idf.py --version 2>&1
    if ($LASTEXITCODE -eq 0) {
        Write-Host " OK ($idfVer)" -ForegroundColor Green
        $idfFound = $true
    } else {
        Write-Host " NOT FOUND in current shell" -ForegroundColor Yellow
    }
} catch {
    Write-Host " NOT FOUND in current shell" -ForegroundColor Yellow
}

if (-not $idfFound) {
    Write-Host "     Tip: Open ESP-IDF PowerShell prompt or run export.ps1 from your ESP-IDF install path." -ForegroundColor Gray
    Write-Host "     If not installed, install ESP-IDF v5.3+ or v6.1 from: https://dl.espressif.com/dl/esp-idf/" -ForegroundColor Gray
}

# 4. USB COM Ports (CP210x / CH340 / ESP32-S3 USB JTAG)
Write-Host "[4/5] Checking Connected Serial/COM Devices..."
$ports = Get-CimInstance Win32_SerialPort -ErrorAction SilentlyContinue
if ($ports) {
    foreach ($p in $ports) {
        Write-Host "     Found: $($p.DeviceID) - $($p.Name)" -ForegroundColor Green
    }
} else {
    Write-Host "     No hardware COM ports detected currently." -ForegroundColor Yellow
    Write-Host "     (Connect ESP32-S3 via USB-C cable to bring up hardware)" -ForegroundColor Gray
}

# 5. Project Dependencies Check
Write-Host "[5/5] Checking Project Dependencies Configuration..."
$manifest = "main/idf_component.yml"
if (Test-Path $manifest) {
    Write-Host "     IDF Component Manifest ($manifest): OK" -ForegroundColor Green
    Get-Content $manifest | ForEach-Object { Write-Host "       $_" -ForegroundColor DarkGray }
} else {
    Write-Host "     Missing $manifest" -ForegroundColor Red
}

Write-Host "`n==================================================" -ForegroundColor Cyan
Write-Host " Summary & Next Steps:" -ForegroundColor Cyan
Write-Host " 1. Desktop Simulation (No hardware needed):"
Write-Host "    powershell tools/run_simulator.ps1" -ForegroundColor Yellow
Write-Host " 2. Build for ESP32-S3 Hardware:"
Write-Host "    idf.py set-target esp32s3" -ForegroundColor Yellow
Write-Host "    idf.py build" -ForegroundColor Yellow
Write-Host " 3. Flash & Monitor:"
Write-Host "    idf.py -p COMx flash monitor" -ForegroundColor Yellow
Write-Host "==================================================" -ForegroundColor Cyan

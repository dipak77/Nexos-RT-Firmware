# Smart Device Platform - Production Hardware Flasher & Auto Setup
[CmdletBinding()]
param(
    [string]$Port,
    [int]$Baud = 921600,
    [switch]$Erase,
    [switch]$NoMonitor
)

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path

Write-Host "=========================================================" -ForegroundColor Cyan
Write-Host " Smart Device Firmware v1.0.0 - Hardware Auto Flasher" -ForegroundColor Cyan
Write-Host " Target MCU: ESP32-S3 (WROOM-1 / DevKitC-1 v1.1)" -ForegroundColor Cyan
Write-Host " Target LCD: GC9A01 240x240 Circular Display" -ForegroundColor Cyan
Write-Host "=========================================================" -ForegroundColor Cyan

# 1. Check Python & Esptool
Write-Host "`n[1/4] Checking Flashing Tools..." -ForegroundColor Yellow
$esptoolCmd = "esptool.py"
try {
    $null = esptool.py version 2>&1
    Write-Host "      Found esptool.py in PATH" -ForegroundColor Green
} catch {
    try {
        $null = python -m esptool version 2>&1
        $esptoolCmd = "python -m esptool"
        Write-Host "      Found python -m esptool" -ForegroundColor Green
    } catch {
        Write-Host "      esptool not found. Installing via pip..." -ForegroundColor Yellow
        python -m pip install esptool
        $esptoolCmd = "python -m esptool"
    }
}

# 2. Check Binary Files
Write-Host "`n[2/4] Verifying Firmware Binary Package..." -ForegroundColor Yellow
$bootloaderBin = Join-Path $scriptDir "bootloader.bin"
$partitionBin  = Join-Path $scriptDir "partition-table.bin"
$otaDataBin    = Join-Path $scriptDir "ota_data_initial.bin"
$appBin        = Join-Path $scriptDir "smart_device_firmware.bin"
$mergedBin     = Join-Path $scriptDir "firmware_all_in_one.bin"

$useMerged = (Test-Path $mergedBin)
$useSeparate = ((Test-Path $bootloaderBin) -and (Test-Path $partitionBin) -and (Test-Path $appBin))

if (-not $useMerged -and -not $useSeparate) {
    Write-Host "      [ERROR] Firmware binary files not found in $scriptDir" -ForegroundColor Red
    Write-Host "      Expected: firmware_all_in_one.bin OR (bootloader.bin, partition-table.bin, smart_device_firmware.bin)" -ForegroundColor Red
    exit 1
}

Write-Host "      Binary package verified: OK" -ForegroundColor Green

# 3. Auto-Detect ESP32-S3 Serial Port
Write-Host "`n[3/4] Detecting Connected ESP32-S3 Device..." -ForegroundColor Yellow
if (-not $Port) {
    $ports = Get-CimInstance Win32_SerialPort -ErrorAction SilentlyContinue
    if ($ports) {
        $Port = $ports[0].DeviceID
        Write-Host "      Auto-detected COM port: $Port ($($ports[0].Name))" -ForegroundColor Green
    } else {
        $Port = Read-Host "      No active COM port detected. Please enter your COM port (e.g. COM7)"
        if (-not $Port) { $Port = "COM7" }
    }
} else {
    Write-Host "      Using specified port: $Port" -ForegroundColor Cyan
}

# 4. Flashing Flash Memory
Write-Host "`n[4/4] Flashing Firmware to ESP32-S3 on $Port @ $Baud baud..." -ForegroundColor Yellow

if ($Erase) {
    Write-Host "      Erasing entire chip flash..." -ForegroundColor DarkYellow
    if ($esptoolCmd -eq "esptool.py") {
        esptool.py --chip esp32s3 -p $Port erase-flash
    } else {
        python -m esptool --chip esp32s3 -p $Port erase-flash
    }
}

$flashSuccess = $false

if ($useMerged) {
    Write-Host "      Writing single all-in-one merged binary at 0x0..." -ForegroundColor Cyan
    if ($esptoolCmd -eq "esptool.py") {
        esptool.py --chip esp32s3 -p $Port -b $Baud --before default-reset --after hard-reset write-flash -z 0x0 $mergedBin
    } else {
        python -m esptool --chip esp32s3 -p $Port -b $Baud --before default-reset --after hard-reset write-flash -z 0x0 $mergedBin
    }
    if ($LASTEXITCODE -eq 0) { $flashSuccess = $true }
} else {
    Write-Host "      Writing multi-part binary package..." -ForegroundColor Cyan
    $flashArgs = @(
        "--chip", "esp32s3",
        "-p", $Port,
        "-b", $Baud.ToString(),
        "--before", "default-reset",
        "--after", "hard-reset",
        "write-flash", "-z",
        "0x0000", $bootloaderBin,
        "0x8000", $partitionBin
    )
    if (Test-Path $otaDataBin) {
        $flashArgs += @("0xF000", $otaDataBin)
    }
    $flashArgs += @("0x10000", $appBin)

    if ($esptoolCmd -eq "esptool.py") {
        & esptool.py $flashArgs
    } else {
        & python -m esptool $flashArgs
    }
    if ($LASTEXITCODE -eq 0) { $flashSuccess = $true }
}

if ($flashSuccess) {
    Write-Host "`n=========================================================" -ForegroundColor Green
    Write-Host " [SUCCESS] Smart Device Firmware Flashed Successfully!  " -ForegroundColor Green
    Write-Host "=========================================================" -ForegroundColor Green

    if (-not $NoMonitor) {
        Write-Host "`nLaunching Serial Monitor on $Port @ 115200 baud..." -ForegroundColor Cyan
        Write-Host "Press Ctrl+C or Ctrl+] to exit monitor." -ForegroundColor Gray
        try {
            python -m serial.tools.miniterm $Port 115200
        } catch {
            Write-Host "Serial monitor closed." -ForegroundColor Gray
        }
    }
} else {
    Write-Host "`n=========================================================" -ForegroundColor Red
    Write-Host " [FAILED] Flash operation encountered an error.          " -ForegroundColor Red
    Write-Host " Tips:" -ForegroundColor Yellow
    Write-Host " 1. Hold the BOOT button on the ESP32-S3 while plugging in USB." -ForegroundColor Yellow
    Write-Host " 2. Ensure no other application (PuTTY, Arduino, etc.) has $Port open." -ForegroundColor Yellow
    Write-Host "=========================================================" -ForegroundColor Red
}

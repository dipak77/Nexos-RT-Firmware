# Smart Device Platform - Production Hardware Flasher & Auto Setup
[CmdletBinding()]
param(
    [string]$Port,
    [int]$Baud = 460800,
    [switch]$Erase,
    [switch]$NoMonitor
)

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path

Write-Host "=========================================================" -ForegroundColor Cyan
Write-Host " Smart Device Firmware v1.2.0 - Hardware Auto Flasher" -ForegroundColor Cyan
Write-Host " Target MCU: ESP32-S3 (WROOM-1 / DevKitC-1 v1.1)" -ForegroundColor Cyan
Write-Host " Target LCD: GC9A01 240x240 Circular Display" -ForegroundColor Cyan
Write-Host "=========================================================" -ForegroundColor Cyan

# 1. Check Python & Esptool
Write-Host "`n[1/4] Checking Flashing Tools..." -ForegroundColor Yellow
$esptoolMode = $null
$esptoolPath = Get-Command esptool -ErrorAction SilentlyContinue
if (-not $esptoolPath) {
    $esptoolPath = Get-Command esptool.py -ErrorAction SilentlyContinue
}

if ($esptoolPath) {
    & $esptoolPath.Source version *> $null
    if ($LASTEXITCODE -eq 0) {
        $esptoolMode = "executable"
        Write-Host "      Found $($esptoolPath.Name) in PATH" -ForegroundColor Green
    }
}

$pythonPath = Get-Command python -ErrorAction SilentlyContinue
if (-not $esptoolMode -and $pythonPath) {
    & $pythonPath.Source -m esptool version *> $null
    if ($LASTEXITCODE -eq 0) {
        $esptoolMode = "python"
        Write-Host "      Found python -m esptool" -ForegroundColor Green
    }
}

# PlatformIO can leave a usable esptool package in its ESP-IDF environment even
# when that virtual environment's Python launcher points at a removed base Python.
# Reuse those pure-Python packages with the active interpreter as a local fallback.
if (-not $esptoolMode -and $pythonPath) {
    $pioEnvRoot = Join-Path $env:USERPROFILE ".platformio\penv"
    $pioSitePackages = Get-ChildItem -LiteralPath $pioEnvRoot -Directory -ErrorAction SilentlyContinue |
        ForEach-Object { Join-Path $_.FullName "Lib\site-packages" } |
        Where-Object { Test-Path (Join-Path $_ "esptool") } |
        Select-Object -First 1
    if ($pioSitePackages) {
        if ($env:PYTHONPATH) {
            $env:PYTHONPATH = "$pioSitePackages;$($env:PYTHONPATH)"
        } else {
            $env:PYTHONPATH = $pioSitePackages
        }
        & $pythonPath.Source -m esptool version *> $null
        if ($LASTEXITCODE -eq 0) {
            $esptoolMode = "python"
            Write-Host "      Found esptool in the PlatformIO environment" -ForegroundColor Green
        }
    }
}

if (-not $esptoolMode) {
    Write-Host "      [ERROR] A working esptool installation was not found." -ForegroundColor Red
    Write-Host "      Install it with: python -m pip install esptool" -ForegroundColor Yellow
    exit 1
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
    $ports = @(Get-CimInstance Win32_SerialPort -ErrorAction SilentlyContinue)
    $usbSerialPorts = @($ports | Where-Object {
        $_.Name -match "CP210|Silicon Labs|USB.*Serial" -or
        $_.PNPDeviceID -match "VID_10C4&PID_EA60"
    })
    if ($usbSerialPorts.Count -eq 1) {
        $Port = $usbSerialPorts[0].DeviceID
        Write-Host "      Auto-detected COM port: $Port ($($usbSerialPorts[0].Name))" -ForegroundColor Green
    } elseif ($ports.Count -eq 1) {
        $Port = $ports[0].DeviceID
        Write-Host "      Using the only serial port: $Port ($($ports[0].Name))" -ForegroundColor Green
    } else {
        Write-Host "      [ERROR] Could not uniquely identify the ESP32-S3 serial port." -ForegroundColor Red
        if ($ports.Count -gt 0) {
            $ports | ForEach-Object { Write-Host "      $($_.DeviceID): $($_.Name)" -ForegroundColor Yellow }
        }
        Write-Host "      Re-run with an explicit port, for example: -Port COM5" -ForegroundColor Yellow
        exit 1
    }
} else {
    Write-Host "      Using specified port: $Port" -ForegroundColor Cyan
}

# 4. Flashing Flash Memory
Write-Host "`n[4/4] Flashing Firmware to ESP32-S3 on $Port @ $Baud baud..." -ForegroundColor Yellow

if ($Erase) {
    Write-Host "      Erasing entire chip flash..." -ForegroundColor DarkYellow
    if ($esptoolMode -eq "executable") {
        & $esptoolPath.Source --chip esp32s3 -p $Port erase-flash
    } else {
        & $pythonPath.Source -m esptool --chip esp32s3 -p $Port erase-flash
    }
    if ($LASTEXITCODE -ne 0) {
        Write-Host "      [ERROR] Chip erase failed; flash was not attempted." -ForegroundColor Red
        exit $LASTEXITCODE
    }
}

$flashSuccess = $false

if ($useMerged) {
    Write-Host "      Writing single all-in-one merged binary at 0x0..." -ForegroundColor Cyan
    if ($esptoolMode -eq "executable") {
        & $esptoolPath.Source --chip esp32s3 -p $Port -b $Baud --before default-reset --after hard-reset write-flash -z 0x0 $mergedBin
    } else {
        & $pythonPath.Source -m esptool --chip esp32s3 -p $Port -b $Baud --before default-reset --after hard-reset write-flash -z 0x0 $mergedBin
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
    $flashArgs += @("0x20000", $appBin)

    if ($esptoolMode -eq "executable") {
        & $esptoolPath.Source $flashArgs
    } else {
        & $pythonPath.Source -m esptool $flashArgs
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
    exit 1
}

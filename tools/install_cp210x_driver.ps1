# Automated CP210x Driver Installer for ESP32-S3
[CmdletBinding()]
param()

Write-Host "=========================================================" -ForegroundColor Cyan
Write-Host " Silicon Labs CP210x USB to UART Driver Installer" -ForegroundColor Cyan
Write-Host " Target: Fix Device Error Code 28 (Driver Missing)" -ForegroundColor Cyan
Write-Host "=========================================================" -ForegroundColor Cyan

$driverDir = Join-Path (Split-Path -Parent $MyInvocation.MyCommand.Path) "cp210x_driver"
$zipPath = Join-Path (Split-Path -Parent $MyInvocation.MyCommand.Path) "CP210x_Universal_Windows_Driver.zip"
$driverUrl = "https://www.silabs.com/documents/public/software/CP210x_Universal_Windows_Driver.zip"

# 1. Download driver zip
if (-not (Test-Path $driverDir)) {
    New-Item -ItemType Directory -Path $driverDir -Force | Out-Null
}

Write-Host "`n[1/3] Downloading official Silicon Labs CP210x driver package..." -ForegroundColor Yellow
try {
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    Invoke-WebRequest -Uri $driverUrl -OutFile $zipPath -UserAgent "Mozilla/5.0"
    Write-Host "      Download completed successfully." -ForegroundColor Green
} catch {
    Write-Host "      Download failed: $_" -ForegroundColor Red
    Write-Host "      Please download manually from: https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers" -ForegroundColor Yellow
    exit 1
}

# 2. Extract driver
Write-Host "`n[2/3] Extracting driver files..." -ForegroundColor Yellow
try {
    Expand-Archive -Path $zipPath -DestinationPath $driverDir -Force
    Write-Host "      Extracted to $driverDir" -ForegroundColor Green
} catch {
    Write-Host "      Extraction failed: $_" -ForegroundColor Red
    exit 1
}

# 3. Install driver with pnputil
Write-Host "`n[3/3] Installing CP210x driver into Windows Driver Store..." -ForegroundColor Yellow
$infFile = Join-Path $driverDir "silabser.inf"

if (Test-Path $infFile) {
    Write-Host "      Running: pnputil /add-driver `"$infFile`" /install" -ForegroundColor Cyan
    $pnpOutput = pnputil.exe /add-driver "$infFile" /install 2>&1
    Write-Host $pnpOutput

    Write-Host "`nChecking device status after driver installation..." -ForegroundColor Yellow
    Start-Sleep -Seconds 2
    
    $pnp = Get-PnpDevice -PresentOnly -ErrorAction SilentlyContinue | Where-Object {
        $_.InstanceId -match "VID_10C4" -or $_.Class -eq "Ports"
    }

    if ($pnp) {
        Write-Host "`n=========================================================" -ForegroundColor Green
        Write-Host " [SUCCESS] CP210x Driver Installed Successfully!" -ForegroundColor Green
        Write-Host "=========================================================" -ForegroundColor Green
        $pnp | Format-Table Status, Class, FriendlyName, InstanceId -AutoSize
    } else {
        Write-Host "`nDriver added to system. If port does not appear immediately, please unplug and replug the ESP32-S3 USB cable." -ForegroundColor Yellow
    }
} else {
    Write-Host "      [ERROR] silabser.inf not found in $driverDir" -ForegroundColor Red
}

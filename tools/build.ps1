# Smart Device Firmware - Build Tool
[CmdletBinding()]
param(
    [switch]$FullClean,
    [switch]$SetTarget
)

$target = "esp32s3"

if ($SetTarget) {
    Write-Host "Setting target to $target..." -ForegroundColor Cyan
    idf.py set-target $target
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Failed to set target to $target" -ForegroundColor Red
        exit 1
    }
}

if ($FullClean) {
    Write-Host "Running full clean..." -ForegroundColor Cyan
    idf.py fullclean
}

Write-Host "Building Smart Device Firmware ($target)..." -ForegroundColor Cyan
$startTime = Get-Date

idf.py build

if ($LASTEXITCODE -eq 0) {
    $elapsed = (Get-Date) - $startTime
    Write-Host "`n[SUCCESS] Build completed in $($elapsed.TotalSeconds.ToString('F1')) seconds!" -ForegroundColor Green
    $binPath = "build/smart_device_firmware.bin"
    if (Test-Path $binPath) {
        $binSize = (Get-Item $binPath).Length
        Write-Host "Binary: $binPath ($([math]::Round($binSize/1KB, 1)) KB)" -ForegroundColor Green
    }
} else {
    Write-Host "`n[FAILED] Build failed with exit code $LASTEXITCODE" -ForegroundColor Red
    Write-Host "Tip: Ensure ESP-IDF environment is active (run export.ps1) and run 'idf.py set-target esp32s3'." -ForegroundColor Yellow
}

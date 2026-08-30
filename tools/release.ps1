# Smart Device Firmware - Release Packaging Tool
[CmdletBinding()]
param(
    [string]$Version = "1.2.0"
)

Write-Host "Creating release package v$Version..." -ForegroundColor Cyan

pio run -e esp32s3_arduino
if ($LASTEXITCODE -ne 0) {
    Write-Host "[FAILED] PlatformIO build failed" -ForegroundColor Red
    exit $LASTEXITCODE
}

python tools/generate_release_binaries.py
if ($LASTEXITCODE -ne 0) {
    Write-Host "[FAILED] Release packaging failed" -ForegroundColor Red
    exit $LASTEXITCODE
}

$bin = "release/smart_device_firmware.bin"
$outDir = "release"
if (-not (Test-Path $outDir)) {
    New-Item -ItemType Directory -Path $outDir | Out-Null
}

if (Test-Path $bin) {
    $outBin = "$outDir/smart_device_firmware-v$Version.bin"
    Copy-Item $bin $outBin -Force
    $size = (Get-Item $outBin).Length
    Write-Host "[SUCCESS] Release artifact created: $outBin ($([math]::Round($size/1KB, 1)) KB)" -ForegroundColor Green
} else {
    Write-Host "[FAILED] Build artifact not found at $bin" -ForegroundColor Red
}

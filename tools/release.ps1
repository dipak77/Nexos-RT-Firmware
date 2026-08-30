# Smart Device Firmware - Release Packaging Tool
[CmdletBinding()]
param(
    [string]$Version = "1.0.0"
)

Write-Host "Creating release package v$Version..." -ForegroundColor Cyan

idf.py build

$bin = "build/smart_device_firmware.bin"
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

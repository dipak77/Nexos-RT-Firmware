@echo off
setlocal enabledelayedexpansion

echo =========================================================
echo  Smart Device Firmware v1.2.0 - 1-Click Hardware Flasher
echo  Target: ESP32-S3 ^| Display: GC9A01 240x240 Round TFT
echo =========================================================
echo.

cd /d "%~dp0"

powershell -ExecutionPolicy Bypass -File "%~dp0flash_device.ps1"

if %ERRORLEVEL% neq 0 (
    echo.
    echo [ERROR] Flash failed. Please check your USB-C cable and COM connection.
    pause
)

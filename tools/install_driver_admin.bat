@echo off
setlocal

echo =========================================================
echo  Silicon Labs CP210x Driver Installer (Administrator)
echo =========================================================
echo.

:: Check for administrative rights
net session >nul 2>&1
if %errorlevel% neq 0 (
    echo Requesting Administrator privileges...
    powershell -Command "Start-Process cmd -ArgumentList '/c \"\"%~f0\"\"' -Verb RunAs"
    exit /b
)

echo Installing CP210x driver to Windows Driver Store...
pnputil /add-driver "%~dp0cp210x_driver\silabser.inf" /install

if %errorlevel% equ 0 (
    echo.
    echo =========================================================
    echo  [SUCCESS] CP210x Driver Installed Successfully!
    echo  Please unplug and replug your ESP32-S3 USB cable.
    echo =========================================================
) else (
    echo.
    echo [ERROR] Driver installation failed.
)

pause

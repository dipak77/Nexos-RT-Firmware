# Smart Device Firmware - Serial Monitor Tool
[CmdletBinding()]
param(
    [string]$Port = "COM7",
    [int]$Baud = 115200
)

$foundPorts = Get-CimInstance Win32_SerialPort -ErrorAction SilentlyContinue
if ($foundPorts -and ($Port -eq "COM7")) {
    $Port = $foundPorts[0].DeviceID
    Write-Host "Auto-detected port: $Port" -ForegroundColor Cyan
}

Write-Host "Opening ESP-IDF serial monitor on $Port @ $Baud baud..." -ForegroundColor Cyan
Write-Host "Press Ctrl+] to exit monitor." -ForegroundColor Gray

idf.py -p $Port -b $Baud monitor

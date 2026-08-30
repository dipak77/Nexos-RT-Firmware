# Smart Device Firmware - Flash Erase Tool
[CmdletBinding()]
param(
    [string]$Port = "COM7"
)

$foundPorts = Get-CimInstance Win32_SerialPort -ErrorAction SilentlyContinue
if ($foundPorts -and ($Port -eq "COM7")) {
    $Port = $foundPorts[0].DeviceID
}

Write-Host "Erasing flash on $Port..." -ForegroundColor Yellow
idf.py -p $Port erase-flash

if ($LASTEXITCODE -eq 0) {
    Write-Host "[SUCCESS] Flash erased successfully on $Port" -ForegroundColor Green
} else {
    Write-Host "[FAILED] Flash erase failed on $Port" -ForegroundColor Red
}

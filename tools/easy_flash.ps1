# Smart Device Platform - 1-Click Hardware Flasher & Auto Setup (Project Root)
[CmdletBinding()]
param(
    [string]$Port,
    [int]$Baud = 921600,
    [switch]$Erase,
    [switch]$NoMonitor
)

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$releaseDir = Join-Path (Split-Path -Parent $scriptDir) "release"

$ps1 = Join-Path $releaseDir "flash_device.ps1"

$argsList = @()
if ($Port) { $argsList += @("-Port", $Port) }
if ($Baud) { $argsList += @("-Baud", $Baud) }
if ($Erase) { $argsList += "-Erase" }
if ($NoMonitor) { $argsList += "-NoMonitor" }

powershell -ExecutionPolicy Bypass -File $ps1 @argsList

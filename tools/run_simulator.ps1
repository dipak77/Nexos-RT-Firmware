# Launch Desktop Simulator for Smart Device Firmware
[CmdletBinding()]
param(
    [switch]$CliOnly,
    [switch]$Web
)

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoDir = Split-Path -Parent $scriptDir

Set-Location $repoDir

if ($CliOnly) {
    python simulator/run_simulator.py --cli
} elseif ($Web) {
    python simulator/run_simulator.py --web
} else {
    python simulator/run_simulator.py
}

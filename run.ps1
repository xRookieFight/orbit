param([switch]$Headless)

$ErrorActionPreference = "Stop"
$arg = ""
if ($Headless) { $arg = "headless" }

Write-Host "Building and launching Orbit 2.0 via WSL + QEMU..."
wsl.exe bash -c "cat /mnt/c/Users/xRookieFight/Desktop/Hepsi/orbit/run.sh | tr -d '\015' | bash -s $arg"

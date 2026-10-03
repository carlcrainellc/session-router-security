# Not the primary path. Double-click Start-Session-Router.cmd instead.
# That starter launches session-router.exe and sets sr-tun0 DNS to 127.0.0.1.
# This file only remains for operators who already started the exe themselves.

$ErrorActionPreference = 'Stop'
$Adapter = 'sr-tun0'
$Dns = '127.0.0.1'
$TimeoutSec = 60

Write-Host "Waiting for adapter $Adapter (up to ${TimeoutSec}s)..."
$deadline = (Get-Date).AddSeconds($TimeoutSec)
while ((Get-Date) -lt $deadline) {
    $iface = Get-NetAdapter -Name $Adapter -ErrorAction SilentlyContinue
    if ($iface) { break }
    Start-Sleep -Seconds 1
}
if (-not (Get-NetAdapter -Name $Adapter -ErrorAction SilentlyContinue)) {
    Write-Error "Adapter $Adapter not found. Start session-router.exe first, then re-run."
    exit 1
}

Write-Host "Setting DNS on $Adapter to $Dns ..."
netsh interface ip set dns name="$Adapter" static $Dns primary validate=no
if ($LASTEXITCODE -ne 0) {
    Write-Error "netsh set dns failed (exit $LASTEXITCODE)."
    exit $LASTEXITCODE
}

Write-Host "Current DNS:"
netsh interface ip show dns name="$Adapter"
Write-Host "Pre-flight done."

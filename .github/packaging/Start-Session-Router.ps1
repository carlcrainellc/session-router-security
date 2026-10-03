# Session Router one-click starter (body).
# Double-click Start-Session-Router.cmd. That file asks for Administrator
# (UAC) first, because creating sr-tun0 and setting its DNS need admin.
# You do not run netsh by hand.
#
# Empty upstream= in session-router.ini is valid. This script does not read
# upstream= and does not refuse to start when it is empty.
# Packaged Exit stays off. This script does not enable Exit.

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location -LiteralPath $Root

$Exe = Join-Path $Root 'session-router.exe'
$Ini = Join-Path $Root 'session-router.ini'
$Dll = Join-Path $Root 'wintun.dll'
foreach ($p in @($Exe, $Ini, $Dll)) {
    if (-not (Test-Path -LiteralPath $p)) {
        Write-Host "Missing file: $p"
        exit 1
    }
}

$Adapter = 'sr-tun0'
$Dns = '127.0.0.1'

Write-Host "Starting session-router.exe -c session-router.ini"
$proc = Start-Process -FilePath $Exe -ArgumentList @('-c', $Ini) -WorkingDirectory $Root -PassThru
if (-not $proc) {
    Write-Host 'Failed to start session-router.exe'
    exit 1
}

Write-Host "Waiting for adapter $Adapter (up to 120 seconds)..."
$deadline = (Get-Date).AddSeconds(120)
$found = $false
while ((Get-Date) -lt $deadline) {
    if ($proc.HasExited) {
        Write-Host "session-router.exe exited early (code $($proc.ExitCode))."
        exit 1
    }
    $iface = Get-NetAdapter -Name $Adapter -ErrorAction SilentlyContinue
    if ($iface) { $found = $true; break }
    Start-Sleep -Seconds 1
}
if (-not $found) {
    Write-Host "Adapter $Adapter did not appear. session-router.exe pid $($proc.Id) is still the process to watch."
    exit 1
}

Write-Host "Setting DNS on $Adapter to $Dns"
& netsh interface ip set dns name="$Adapter" static $Dns primary validate=no
if ($LASTEXITCODE -ne 0) {
    Write-Host "Setting DNS failed (netsh exit $LASTEXITCODE). Approve UAC / run the .cmd as Administrator."
    exit $LASTEXITCODE
}

Write-Host "IPv4 DNS on ${Adapter}:"
Get-DnsClientServerAddress -InterfaceAlias $Adapter -AddressFamily IPv4 | Format-List
Write-Host "Done. session-router.exe is still running (pid $($proc.Id)). Leave that window open. You can close the starter window."
exit 0

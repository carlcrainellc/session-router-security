# Windows package — run steps

Release zip: statically linked `session-router.exe`, official `wintun.dll`, `session-router.ini`, `bootstrap.signed`, and the double-click starter. Full guide: [docs/RUN-WINDOWS.md](../../docs/RUN-WINDOWS.md).

1. Unpack the zip so the files sit in one folder.
2. **Double-click `Start-Session-Router.cmd`.** Approve the Administrator (UAC) prompt. Creating `sr-tun0` and setting its DNS need admin. The starter does that. You do not run `netsh` by hand.
3. The starter runs `session-router.exe -c session-router.ini`, waits until `sr-tun0` exists, and sets that adapter’s DNS to `127.0.0.1`.
4. Empty `upstream=` is valid. The starter does not refuse to start because upstream is empty. Keep Exit disabled (`enable=false`).

Optional check after the starter finishes:

```powershell
Get-DnsClientServerAddress -InterfaceAlias sr-tun0 -AddressFamily IPv4
```

Expected: `127.0.0.1`.

See also [windows-client.md](../../docs/windows-client.md) and [docs/RUN-WINDOWS.md](../../docs/RUN-WINDOWS.md).

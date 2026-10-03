# Running the Windows client

This document describes how to run the MinGW-built Windows client package.

The release zip ships a **statically linked** `session-router.exe` plus official
**`wintun.dll`**. MinGW C++/runtime DLLs are **not** required for that package
(they are linked into the exe). Do not fetch MinGW runtimes from an Ubuntu
sysroot for the release zip.

## What the release zip contains

- `session-router.exe` — statically linked client binary
- `wintun.dll` — official Wintun amd64 (427552 bytes)
- `session-router.ini` — packaged client defaults (Exit off; DNS on 127.0.0.1:53; empty `upstream=`)
- `bootstrap.signed` — signed bootstrap relay contacts
- `Start-Session-Router.cmd` — **double-click this** to start the client and set tunnel DNS
- `Start-Session-Router.ps1` — starter body (launched by the `.cmd`; do not use this as a separate path)
- `RUN-WINDOWS.md` / `windows-client.md` — run docs

The default click is a **client**, not a hidden-service host. Exit stays off.

## Runtime dependency

**Only `wintun.dll` is required** next to the exe (already included in the
release zip).

- Official download (if you replace it): https://www.wintun.net/builds/wintun-0.14.1.zip
- Project page: https://www.wintun.net/
- Extract **`wintun/bin/amd64/wintun.dll`** for 64-bit Windows.
- Expected size: **427552** bytes.
- Verify the PE machine type is amd64 (`0x8664`). Do not use the x86 or arm64
  builds of the DLL with this client.

Example verification (PowerShell):

```powershell
(Get-Item .\wintun.dll).Length   # expect 427552
```

If you build from source yourself and produce a **dynamically** linked binary,
you would need matching MinGW-w64 POSIX runtimes from the same toolchain that
built the exe. That path is **not** what the release zip ships.

## Quick start

1. Unpack the release zip into a folder. Keep `session-router.exe`, `wintun.dll`,
   `session-router.ini`, `bootstrap.signed`, and `Start-Session-Router.cmd` side
   by side.
2. **Double-click `Start-Session-Router.cmd`.**
3. Approve the Administrator (UAC) prompt. Creating the `sr-tun0` adapter and
   setting its DNS need admin. The starter does both. You do not run `netsh`
   yourself.
4. The starter runs `session-router.exe -c session-router.ini`, waits until
   `sr-tun0` exists, and sets that adapter’s DNS to `127.0.0.1`.
5. Leave the Session Router window open. You can close the starter window.
6. Leave Exit **off**. An empty `upstream=` is normal and does not block start.
   Do not set a clearnet DNS upstream.

## Tunnel DNS (done by the starter)

`.sesh` / `.snode` names resolve only if `sr-tun0` uses the local Session Router
resolver at `127.0.0.1`. The double-click starter sets that after the adapter
appears. That is the primary path.

Optional check (PowerShell, after the starter finishes):

```powershell
Get-DnsClientServerAddress -InterfaceAlias sr-tun0 -AddressFamily IPv4
```

Expected: `127.0.0.1`.

## Bootstrap modes

See [windows-client.md](windows-client.md). Defaults use **normal** mode. Optional
**local** and **chain3** modes are documented there.

## Exit routing

Exit stays **disabled** in the packaged ini (`enable=false`, `auto-routing=false`).
Do not enable it for everyday use. See [exit-integrity.md](exit-integrity.md) and
the README Exit status table (code fixes do not turn Exit on).

## Authenticity checklist

1. Confirm the git commit SHA you intended (release notes name the commit SHA).
2. Hash `session-router.exe` locally and keep that hash with the SHA.
3. Confirm `wintun.dll` is **427552** bytes and amd64 from the official Wintun zip
   (or the copy shipped in the release zip).
4. Confirm packaged `session-router.ini` still has `listen=127.0.0.1:53`,
   empty `upstream=`, `enable=false`, and `[api] enabled=false` unless you
   knowingly changed them.

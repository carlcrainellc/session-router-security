# Running the Windows client

This document describes how to run the MinGW-built Windows client package.

The **v0.1.0 release zip** ships a **statically linked** `session-router.exe`
plus official **`wintun.dll`**. MinGW C++/runtime DLLs are **not** required for
that package (they are linked into the exe). Do not fetch MinGW runtimes from
an Ubuntu sysroot for the release zip.

## What the release zip contains

- `session-router.exe` — statically linked client binary
- `wintun.dll` — official Wintun amd64 (see DEPENDENCIES.txt in the zip)
- `session-router.ini` — packaged defaults (Exit off; see warnings in the README)
- `bootstrap.signed` — signed bootstrap relay contacts
- `RUN-WINDOWS.md` / `windows-client.md` / `tunnel-dns-preflight.ps1` — run docs
- `DEPENDENCIES.txt` — URLs and hashes for third-party binaries included

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
built the exe. That path is **not** what the v0.1.0 release zip ships.

## Quick start

1. Unpack the release zip into a folder (`session-router.exe` and `wintun.dll`
   should be side by side).
2. Keep `session-router.ini` and `bootstrap.signed` beside the exe (or set paths
   in the ini).
3. Run `session-router.exe` as Administrator if the TUN driver requires it.
4. **Set tunnel DNS** (required for `.sesh`): see [Tunnel DNS](#tunnel-dns-required-for-sesh) below.
5. Leave Exit **off** unless you understand the risks in the README.

## Tunnel DNS (required for `.sesh`)

After `session-router.exe` starts and the `sr-tun0` adapter appears, you **must**
point that adapter’s DNS at the local Session Router resolver. This step is
**required for `.sesh` / `.snode` names to resolve**. Without it, Windows will
not send Session Router name queries to the client.

```
netsh interface ip set dns name="sr-tun0" static 127.0.0.1 primary validate=no
```

Optional helper (waits for `sr-tun0`, sets DNS, prints current DNS):

```
powershell -ExecutionPolicy Bypass -File tunnel-dns-preflight.ps1
```

(`tunnel-dns-preflight.ps1` ships in the release zip beside `RUN-WINDOWS.md`.)

Verify:

```
netsh interface ip show dns name="sr-tun0"
```

Expected: statically configured DNS server `127.0.0.1`.

More client context: [windows-client.md](windows-client.md).

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
   (or the copy shipped in the release zip — see DEPENDENCIES.txt).
4. Confirm packaged `session-router.ini` still has `enable=false` and
   `auto-routing=false` unless you knowingly changed them.

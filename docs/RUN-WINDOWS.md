# Running the Windows client

This document describes how to run a MinGW-built `session-router.exe` on Windows.
Published build artifacts include the **executable and config/docs only**. They do
**not** ship runtime DLLs. Obtain those from the official sources below.

## What the artifact contains

- `session-router.exe` — the client binary
- `session-router.ini` — packaged defaults (Exit off; see warnings in the README)
- `bootstrap.signed` — signed bootstrap relay contacts
- `RUN-WINDOWS.md` / `windows-client.md` — this guide and client notes

## Runtime dependencies (official sources only)

Place the DLLs next to `session-router.exe` (or elsewhere on `PATH`).

### 1. Wintun (required for the TUN adapter)

- Official download: https://www.wintun.net/builds/wintun-0.14.1.zip
- Project page: https://www.wintun.net/
- Extract **`wintun/bin/amd64/wintun.dll`** for 64-bit Windows.
- Expected size: **427552** bytes.
- Verify the PE machine type is amd64 (`0x8664`). Do not use the x86 or arm64
  builds of the DLL with this client.

Example verification (PowerShell):

```powershell
(Get-Item .\wintun.dll).Length   # expect 427552
```

### 2. MinGW-w64 C++ runtime (required)

The client is cross-built with the **MinGW-w64 POSIX** toolchain. You need these
three DLLs from a matching MinGW-w64 POSIX sysroot (same major GCC series as the
build, currently GCC 14 posix on the reference Ubuntu 22.04 CI image):

| File | Role |
|------|------|
| `libgcc_s_seh-1.dll` | GCC runtime |
| `libstdc++-6.dll` | C++ standard library |
| `libwinpthread-1.dll` | POSIX threads for winpthread |

**Official channels** (pick one; do not copy DLLs from unrelated third-party zips):

1. **Debian / Ubuntu `mingw-w64` packages** (same family as CI):
   - Packages: `g++-mingw-w64-x86-64-posix`, `mingw-w64-x86-64-dev`
   - Typical paths on the build host:
     - `/usr/lib/gcc/x86_64-w64-mingw32/*-posix/libgcc_s_seh-1.dll`
     - `/usr/lib/gcc/x86_64-w64-mingw32/*-posix/libstdc++-6.dll`
     - `/usr/x86_64-w64-mingw32/lib/libwinpthread-1.dll`
2. **MSYS2 mingw-w64** — https://www.msys2.org/ — install the UCRT64 or
   MINGW64 POSIX-compatible toolchain and take the three DLLs from that
   environment’s `bin` directory. Prefer a GCC major version that matches the
   binary’s build notes.

Always prefer the toolchain that produced the binary (or the documented CI
image) so C++ ABI matches.

## Quick start

1. Unpack the artifact into a folder.
2. Add `wintun.dll` and the three MinGW runtime DLLs (see above).
3. Keep `session-router.ini` and `bootstrap.signed` beside the exe (or set paths
   in the ini).
4. Run `session-router.exe` as Administrator if the TUN driver requires it.
5. **Set tunnel DNS** (required for `.sesh`): see [Tunnel DNS](#tunnel-dns-required-for-sesh) below.
6. Leave Exit **off** unless you understand the risks in the README.


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

(`tunnel-dns-preflight.ps1` ships beside `RUN-WINDOWS.md` in the packaging folder
and in the Windows client release zip.)

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
Do not enable it for everyday use. See the README “Exit security flaws” section.


## Authenticity checklist

1. Confirm the git commit SHA you intended (artifact folder name includes it).
2. Hash `session-router.exe` locally and keep that hash with the SHA.
3. Confirm `wintun.dll` is **427552** bytes and amd64 from the official Wintun zip.
4. Take MinGW DLLs only from the official toolchain that matches this build’s GCC
   major (reference CI: Ubuntu `mingw-w64` POSIX / GCC 14).
5. Confirm packaged `session-router.ini` still has `enable=false` and
   `auto-routing=false` unless you knowingly changed them.

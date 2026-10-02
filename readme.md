# Session Router — Security-oriented fork

This repository is a **security-oriented fork** of Session Foundation’s
[session-router](https://github.com/session-foundation/session-router), imported
from the upstream `dev` branch at SHA
`afb98f959f4f7e0ef996aef02fa21a6caf26b1e9`.

Its identity is **security defaults and Exit honesty**. Windows is the primary
build and run target in this guide. A user should be able to clone or
download, run the Windows client, and understand the risks without
upstream-project lore or insider context. Opening pull requests against Session Foundation is
**not** the goal of this repository.

Many of the serious issues documented here (especially Exit routing and related
client-routing behavior) are **not limited to Windows**. They are protocol /
client defaults that apply on other platforms too. Continuing to run the
official Session Foundation build does not remove those risks if Exit is
enabled; this fork changes defaults and documentation so they are harder to
miss.

- **Full client guide:** [docs/windows-client.md](docs/windows-client.md)
- **How to run:** [docs/RUN-WINDOWS.md](docs/RUN-WINDOWS.md)
- **Exit integrity notes:** [docs/exit-integrity.md](docs/exit-integrity.md)
- **Tunnel DNS (required for `.sesh`):** after start,
  `netsh interface ip set dns name="sr-tun0" static 127.0.0.1 primary validate=no`
  — see [docs/RUN-WINDOWS.md](docs/RUN-WINDOWS.md#tunnel-dns-required-for-sesh)
  and packaging `tunnel-dns-preflight.ps1`

Maintained by Carl Craine and OGMax.

Donate (Ethereum): `0x2d81bfecee48de4cf49fddcc9f279970335fe298`

Donate (SESH token on Arbitrum / EVM): `0x8bc20aBA70685634269d1803E7aCC752d985aa44`

**Latest Windows release:** [v0.1.1-windows-client](https://github.com/carlcrainellc/session-router-security/releases/tag/v0.1.1-windows-client)
— tip `06001544c39b032c291242eb91f12f9f80b66c7b`, `session-router.exe` SHA256 `f91736365d8ee91b84a80aec9fa5d70db8fd6e6b0eeca14099250612b24dc86f`. Exit stays off in the packaged defaults.

## Safer defaults in this security fork

These are the packaged defaults for **this** repository
(`session-router-security`), not Session Foundation’s main client. A normal
download or default Windows build from this fork starts with the settings below.

| Setting | This fork’s default build |
|---------|---------------------------|
| Bootstrap (how the client finds the network) | `normal` — live fetch allowed; `local` and `chain3` exist as advanced choices |
| Local control API | **Off**. If you turn it on, it is limited to this machine and needs matching `auth=` |
| Network listen (QUIC) | All interfaces, with an ephemeral port (`0`) — see the bind lesson below |
| Clearnet DNS (non-`.sesh` / `.snode` names) | **Off** (those names get NXDOMAIN) |
| Reachable (advertise as a public relay) | **False** |
| Exit | **Off** (`enable=false`) |
| Auto Exit routing | **Off** (`auto-routing=false`) |
| Windows stay-up helpers | Drain timer, IPv6 soft-fail limited to the Session Router tunnel adapter, libzstd linked |
| What we publish | **Static exe + official `wintun.dll`** (plus ini / bootstrap / docs); no MinGW runtime DLLs in the package |

`mode=chain3` is an **advanced / operator** path: it uses several diverse `rpc=`
seeds and a 2-of-3 reconcile. It is **not** the double-click default today, it
is **not fully tested yet**, and it is **expected to become the default in a
later release**. For a longer defaults comparison, see
[docs/windows-client.md](docs/windows-client.md).

## Security fixes vs Session Foundation

**Exit is not a finished product here.** Packaged clients keep
`[exit] enable=false` and `auto-routing=false`. Code fixes below do **not** turn
Exit on. Do not enable Exit for everyday browsing.

Upstream left Exit reachable as a config toggle without an equally serious
treatment of integrity failures that appear when it is turned on: return traffic
accepted from the wrong peer, clearnet via Exit before the mapped session is
ready, routes brought up without a real gateway, empty or bare
`0.0.0.0/0` maps that silently become full-tunnel, privileged map/unmap without
real credentials, and last-unmap leaving routes installed. This fork fixes those
in client code and keeps the switch off until Exit is actually ready as a
product.

### Fixed compared to the Foundation repo

| Flaw | Scope | Status | What we fixed / what remains |
|------|-------|--------|------------------------------|
| Broker trust / injection | Protocol / client-routing | **Fixed (code)** | Only a mapped Exit may inject return traffic; an arbitrary Exit-policy peer is not treated as a broker. |
| Clearnet via Exit before `EXIT_CAPABLE` | Protocol / client-routing | **Fixed (code)** | Outbound clearnet via Exit waits until the mapped session is Exit-capable. |
| Fail-closed route bring-up | Protocol / client-routing | **Fixed (code)** | Default-via-TUN is refused when there is no real non-TUN gateway or host pins. |
| Empty ranges / bare `0.0.0.0/0` / `::/0` | Protocol / client-routing | **Fixed (code)** | Empty or bare full-tunnel maps require an explicit `full_tunnel` acknowledgement. |
| `[exit] enable` + empty policy | Protocol / client-routing | **Fixed (code)** | Enable with an empty policy refuses startup instead of acting as allow-all. |
| Last unmap leaves routes up | Protocol / client-routing | **Fixed (code)** | Unmapping the last Exit ranges tears the poked routes down. |
| Map / swap / unmap without API credentials | Local API / Exit controls | **Fixed (code)** | Privileged Exit RPCs need matching `[api] auth=` when the API is on (API stays off by default). |
| IPv6 disable / soft-fail hits all adapters | Windows-specific | **Fixed (code)** | IPv6 soft-fail / disable is scoped to the Session Router TUN adapter only. |
| Bad Win32 default next-hop via tunnel | Windows-specific + Exit path | **Fixed (code)** | Win32 default route uses the TUN network address as next hop, with route install checks. |
| OMQ vs packet-path data race on live Exit ranges | Protocol / client-routing | **Still open** | Map/unmap runs on the job queue, but the packet path can still read live ranges without a shared lock. |
| DNS on-link leak (`set_dns_mode` not wired) | Protocol / client-routing | **Still open** | On-link DNS mode is still not wired; DNS can leak onto the wrong path when Exit is on. |

Packaged Exit stays **off by default**. That is deliberate packaging, not a claim
that Exit-on is finished. Rows marked **Fixed (code)** are corrected in this
branch; enabling Exit remains an operator choice with remaining open items above.

Cross-platform: these Exit / client-routing risks are not Windows-only. Official
Foundation builds can still carry them if Exit is enabled.

Details: [docs/exit-integrity.md](docs/exit-integrity.md) and
[docs/windows-client.md](docs/windows-client.md).

## Bind lesson: all-interfaces ephemeral vs “bind loopback”

Upstream packaging and docs left a mess that everyday users should not have to
decode.

**What actually happens:** the client UDP/QUIC socket binds **all interfaces**
(`0.0.0.0`) with an **ephemeral port** (port `0`). That is normal peer
connectivity. The public QUIC socket is supposed to be reachable on the host’s
addresses at whatever port the OS assigns.

**What went wrong in the packaging/docs story:**

- A fixed port on every address (for example `listen=:1191`) was easy to ship as
  a “simple” default. That puts a **stable** UDP listener on all interfaces —
  wider than most desktop users intend, and easy to misread as “the VPN port.”
- Separate confusion pushed “bind loopback” as if that were the safe QUIC fix.
  Binding the QUIC socket to `127.0.0.1` **breaks the client**. Loopback is for
  the local control API and local DNS, not for the onion UDP path.

**What this fork did — and did not do:**

- We did **not** kill the public QUIC socket. We did **not** move QUIC to
  loopback.
- Packaged config **omits** `listen=:1191`. Default is all-interfaces +
  ephemeral port `0`.
- Code refuses a fixed `listen=:PORT` on all interfaces unless the operator sets
  `allow-all-interfaces=true`.
- Docs were corrected so “ephemeral on `0.0.0.0`” is not confused with “localhost
  only.”

Point: upstream left packaging and wording that either exposed a fixed
all-interfaces port or taught the wrong mitigation. This fork locks the safer
default and documents the remaining risks in the open.

## Honest limits

- The first hop still sees an IP on the path to it (protocol fact).
- TUN use on Windows typically needs elevation.
- Upstream protocol limits still apply; this fork hardens defaults, Windows
  packaging, and docs.
- Exit stays **off** here rather than claiming Exit is finished.
- Running a system-wide / commercial VPN under Session Router (for example Nord)
  is **out of scope for now** — future work, not a supported guide here.

## Threat model (short)

**Improves:** safer defaults (API / DNS / reachable / Exit off), honest bind
docs and fixed-port refusal, optional `local` / `chain3` bootstrap for
operators, Windows stay-up fixes, client Exit integrity fixes listed above, and
a release package that ships only official Wintun as a third-party DLL (exe is
statically linked for MinGW runtimes).

**Does not protect against:** a replaced `bootstrap.signed`, first-hop path
visibility, OS/malware compromise, unofficial DLLs beside the exe, or “full
clearnet anonymity via Exit” (Exit is off; open items remain if turned on).

One-pager: [docs/windows-client.md](docs/windows-client.md#threat-model-one-pager).

## Supply chain & authenticity

- Latest release: [v0.1.1-windows-client](https://github.com/carlcrainellc/session-router-security/releases/tag/v0.1.1-windows-client)
  (tip `06001544c39b032c291242eb91f12f9f80b66c7b`; exe SHA256
  `f91736365d8ee91b84a80aec9fa5d70db8fd6e6b0eeca14099250612b24dc86f`).
- Release zip: statically linked `session-router.exe` + official `wintun.dll`
  (and ini / bootstrap / docs). **Only Wintun is required** beside the exe;
  MinGW C++/runtime DLLs are not needed for that package.
- Wintun: https://www.wintun.net/ — amd64 `wintun.dll`, expected size **427552** bytes
  (also listed with hashes in the zip’s DEPENDENCIES.txt).
- Match the release commit SHA / CI artifact name to the git commit; keep a local
  hash of the exe with that SHA. Treat `bootstrap.signed` as a trust root.
  Details: [docs/RUN-WINDOWS.md](docs/RUN-WINDOWS.md).

## Bootstrap modes

`normal` (default), `local` (signed file only), `chain3` (≥3 diverse RPCs,
2-of-3, height-lag cap, no live publisher fetch). Details and trust notes:
[docs/windows-client.md](docs/windows-client.md).

# License

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

```
Copyright © 2024-2025 Session Technology Foundation
Copyright © 2018-2024 The Oxen Project
Copyright © 2018-2022 Jeff Becker
Copyright © 2018-2020 Rick V. (Historical Windows NT port and portions)
```

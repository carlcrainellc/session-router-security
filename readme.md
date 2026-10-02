# Session Router — Windows client tip

Windows client hardening on top of Session Foundation `session-router` `dev`.

- Upstream: [session-foundation/session-router](https://github.com/session-foundation/session-router) branch `dev`
- Imported tip SHA: `afb98f959f4f7e0ef996aef02fa21a6caf26b1e9`
- **Full client guide:** [docs/windows-client.md](docs/windows-client.md)
- **How to run (runtimes):** [docs/RUN-WINDOWS.md](docs/RUN-WINDOWS.md)

Do **not** open PRs from this repository into Session Foundation until an explicit
public flip is named.

## What this tip locks (packaged defaults)

| Lock | Default |
|------|---------|
| Bootstrap | `normal` (live fetch allowed); `local` and `chain3` available as advanced modes |
| Local control API | **off** (loopback/IPC + `auth=` required if enabled) |
| QUIC bind | all interfaces + ephemeral port `0` (honest wording; not “localhost-only”) |
| Clearnet DNS | **off** (NXDOMAIN for non-`.sesh` / `.snode`) |
| Reachable | **false** |
| Exit | **off** (`enable=false`) |
| Auto exit routing | **off** (`auto-routing=false`) |
| Windows stay-up | drain timer, TUN-scoped IPv6 soft-fail, libzstd linked |
| Published artifact | **exe + config/docs only** — no `.dll` files |

`mode=chain3` is an **operator** path (diverse `rpc=` seeds, 2-of-3 reconcile). It is
**not** the double-click default. See the default-vs-developer table in
[docs/windows-client.md](docs/windows-client.md).

## Warning: Exit routing

**Exit routing is not ready for general use.**

The packaged client keeps Exit **off**. Do not turn Exit on for everyday internet
browsing. Deeper Exit work lives elsewhere; this tip keeps Exit disabled.

### Exit security flaws

**Protocol / client-routing class (not Windows-only):** exit broker trust /
injection; EXIT_CAPABLE gate; route bring-up fail-closed; empty or bare
`0.0.0.0/0` without explicit full-tunnel acknowledgement; empty allow-all policy;
API auth for privileged exit controls; unmap must tear routes down.

**Windows-specific:** IPv6 disable must target the Session Router TUN adapter only
(never global); Win32 gateway / next-hop installation must not install a bad
default via the tunnel.

Full write-up: [docs/windows-client.md](docs/windows-client.md).

## Honest limits

- The first hop still sees an IP on the path to it (protocol fact).
- TUN use on Windows typically needs elevation.
- Upstream protocol limits still apply; this tip hardens defaults and packaging.
- Exit stays **off** here rather than claiming Exit is finished.
- Running a system-wide / commercial VPN under Session Router is **out of scope
  for now** (future work; not a supported guide here).

## Threat model (short)

**Improves:** safer defaults (API / DNS / reachable / Exit off), honest bind
docs, optional `local` / `chain3` bootstrap for operators, Windows stay-up fixes,
artifact supply chain that does not ship third-party DLLs.

**Does not protect against:** a replaced `bootstrap.signed`, first-hop path
visibility, OS/malware compromise, unofficial DLLs beside the exe, or “full
clearnet anonymity via Exit” (Exit is off and has known flaws).

One-pager: [docs/windows-client.md](docs/windows-client.md#threat-model-one-pager).

## Supply chain & authenticity

- Artifacts: `session-router.exe`, `session-router.ini`, `bootstrap.signed`, docs —
  **no DLLs**.
- Wintun: https://www.wintun.net/ — amd64 `wintun.dll`, expected size **427552** bytes.
- MinGW runtimes: official mingw-w64 / MSYS2 POSIX sysroot only (see
  [docs/RUN-WINDOWS.md](docs/RUN-WINDOWS.md)).
- Match CI artifact name `session-router-windows-source-<SHA>` to the git commit;
  keep a local hash of the exe with that SHA. Treat `bootstrap.signed` as a trust
  root.

## Bootstrap modes

`normal` (default), `local` (signed file only), `chain3` (≥3 diverse RPCs,
2-of-3, height-lag cap, no live publisher fetch). Details and trust notes:
[docs/windows-client.md](docs/windows-client.md).


---

# Session Router

<!-- [Español](readme_es.md) [Русский](readme_ru.md) [Français](readme_fr.md) -->

This is Session Router: the IP packet onion routing network that powers low-latency anonymous IP
routing.

Session Router is a major component of communications for current and upcoming functionality in
[Session](https://getsession.org), the anonymous, private messenger.

### Installation instructions can be found [here](docs/install.md).

#### You can learn more about the high level, how to use it and the internals of the protocol [here](docs/readme.md)

[![Build Status](https://ci.oxen.rocks/api/badges/session-foundation/session-router/status.svg?ref=refs/heads/dev)](https://ci.oxen.rocks/session-foundation/session-router)

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

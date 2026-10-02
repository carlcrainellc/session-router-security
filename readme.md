# Session Router — Windows client (fork)

This repository is a **Windows-oriented fork** of Session Foundation’s
[session-router](https://github.com/session-foundation/session-router), imported
from the upstream `dev` branch at SHA
`afb98f959f4f7e0ef996aef02fa21a6caf26b1e9`.

It exists so a stranger can download, build, or run a Windows client with safer
defaults and clearer docs — without needing the upstream project’s internal
context. This is a fork; opening pull requests against Session Foundation is
**not** the goal of this repository.

- **Full client guide:** [docs/windows-client.md](docs/windows-client.md)
- **How to run (runtimes):** [docs/RUN-WINDOWS.md](docs/RUN-WINDOWS.md)

## What the default build already locks

| Lock | Default |
|------|---------|
| Bootstrap | `normal` (live fetch allowed); `local` and `chain3` available as advanced modes |
| Local control API | **off** (loopback/IPC + `auth=` required if enabled) |
| QUIC bind | all interfaces + ephemeral port `0` (see bind lesson below) |
| Clearnet DNS | **off** (NXDOMAIN for non-`.sesh` / `.snode`) |
| Reachable | **false** |
| Exit | **off** (`enable=false`) |
| Auto exit routing | **off** (`auto-routing=false`) |
| Windows stay-up | drain timer, TUN-scoped IPv6 soft-fail, libzstd linked |
| Published artifact | **exe + config/docs only** — no `.dll` files |

`mode=chain3` is an **operator** path (diverse `rpc=` seeds, 2-of-3 reconcile). It
is **not** the double-click default. See the default-vs-developer table in
[docs/windows-client.md](docs/windows-client.md).

## Warning: Exit routing

**Exit routing is not ready for general use.**

Exit is the feature that sends ordinary internet traffic out through a Session
Router exit. Upstream shipped client packaging and docs that left Exit reachable
as a config toggle without an equally serious treatment of what still fails when
it is turned on. This fork keeps Exit **off** in the packaged client
(`[exit] enable=false`, `auto-routing=false`) and documents why.

Do **not** turn Exit on for everyday browsing.

### Flaws found

These are concrete failure classes — not vibes. Most are **protocol /
client-routing** issues that any platform inherits if Exit is enabled. A smaller
set is **Windows-specific**.

**Protocol / client-routing (not Windows-only):**

- **Exit broker trust / injection** — traffic can be steered toward an exit that
  was never explicitly mapped; only a mapped exit should be treated as the exit
  broker.
- **EXIT_CAPABLE gate** — clearnet via Exit can proceed before the mapped peer is
  known to be exit-capable.
- **Route bring-up that is not fail-closed** — missing gateway or host pins can
  leave a partial, unsafe route state instead of refusing.
- **Empty or bare default routes** — empty ranges or bare `0.0.0.0/0` / `::/0`
  can become a full tunnel without an explicit full-tunnel acknowledgement.
- **Empty allow-all exit policy** — `enable` with an empty policy can behave as
  allow-all rather than refuse.
- **Privileged Exit controls without real API auth** — map / swap / unmap must
  require admin credentials, not an unlocked local port.
- **Unmap / empty ranges** — the last unmap must tear routes down; otherwise
  traffic can keep using a path the operator thinks is gone.

**Windows-specific:**

- **IPv6 disable scope** — any IPv6 soft-fail or disable for the tunnel must hit
  the Session Router TUN adapter only, never a global “all adapters” change.
- **Gateway / next-hop installation** — Win32 next-hop selection and `route`
  errors must not install a bad default via the tunnel.

Severity in short: several of these are the difference between “Exit is an
advanced experiment” and “a civilian can lose traffic integrity or scope by
flipping a default.” Upstream defaults and docs did not treat that gap as a
ship-blocker. This fork does.

### What this fork fixed

In **defaults, code, and docs** for this Windows client:

- Packaged Exit and auto-routing stay **off**; the danger is called out in the
  README and client guide, not buried in a comment.
- Local control API defaults **off**; if enabled, bind is loopback/IPC only and
  privileged commands (including Exit map/unmap class) need a real `auth=`
  secret.
- Clearnet DNS and inbound `reachable` stay off so a default install does not
  quietly widen exposure while Exit is discussed.
- Windows stay-up work scopes IPv6 soft-fail to the TUN adapter and hardens
  MinGW runtime linkage so the client is less likely to drop for unrelated
  reasons while operators chase Exit myths.
- Docs state plainly that Exit is unfinished — this repository does not pretend
  a config flip equals a finished anonymity product.

### What remains (why Exit stays off)

The protocol-class Exit issues above are **not** claimed fixed here. Fixing
broker trust, EXIT_CAPABLE gating, fail-closed route bring-up, empty-policy
refusal, and unmap teardown is Exit product work — larger than a Windows
packaging fork. Until that lands and is reviewed, **Exit remains disabled** in
the default build. Enabling it yourself is accepting known, documented risk.

Full write-up: [docs/windows-client.md](docs/windows-client.md).

## Bind lesson: all-interfaces ephemeral vs “bind loopback”

Upstream packaging and docs left a mess that civilians should not have to
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
default and says the quiet part out loud.

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
operators, Windows stay-up fixes, artifact supply chain that does not ship
third-party DLLs.

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

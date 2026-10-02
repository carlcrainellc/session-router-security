# Session Router — Windows client tip

Windows client hardening on top of Session Foundation `session-router` `dev`.

- Upstream: [session-foundation/session-router](https://github.com/session-foundation/session-router) branch `dev`
- Imported tip SHA: `afb98f959f4f7e0ef996aef02fa21a6caf26b1e9`
- Client notes: [docs/windows-client.md](docs/windows-client.md)
- How to run on Windows (runtime deps): [docs/RUN-WINDOWS.md](docs/RUN-WINDOWS.md)

Do **not** open PRs from this repository into Session Foundation until an explicit
public flip is named.

## Warning: Exit routing

**Exit routing is not ready for general use.**

The packaged client keeps Exit **off** (`[exit] enable=false`) and automatic exit
routing **off** (`auto-routing=false`). Do not turn Exit on for everyday internet
browsing. Only use Exit in a controlled test if you understand the risk.

### Exit security flaws

These are reasons Exit stays off by default. Many are **protocol / client-routing
class** issues (any platform that enables Exit inherits them). A smaller set is
**Windows-specific**.

**Protocol / client-routing class (not Windows-only):**

- **Exit broker trust / injection** — traffic must only treat a remote as an exit
  broker when that exit is explicitly mapped; otherwise a wrong peer can be
  treated as your exit.
- **EXIT_CAPABLE gate** — outbound clearnet via Exit must wait until the mapped
  exit is actually exit-capable; sending earlier can leak or fail closed poorly.
- **Route bring-up fail-closed** — bringing host routes up without a real gateway
  or without required host pins must fail closed, not partially succeed.
- **Empty or bare default routes** — empty IP ranges or a bare `0.0.0.0/0` (or
  `::/0`) must not silently become “send everything” without an explicit
  full-tunnel acknowledgement.
- **Empty allow-all exit policy** — enabling Exit with an empty policy must not
  mean “allow all”; it must refuse.
- **API auth for privileged exit controls** — mapping / swapping / unmapping exits
  and related route control must require real admin authentication, not an
  unlocked control port.
- **Unmap / empty ranges** — removing the last mapped range must tear routes down;
  leaving a half-up default via the tunnel is unsafe.

**Windows-specific:**

- **IPv6 binding scope** — disabling IPv6 must target the Session Router TUN
  adapter only, never a global “all adapters” disable.
- **Gateway / next-hop installation** — Win32 host-route next-hop selection and
  `route` error handling must not install a bad default via the tunnel.

Until Exit is safe for general use, leave it off. The packaged ini does that for you.

## Runtime dependencies (not bundled)

Build artifacts ship **`session-router.exe` + config/docs only**. They do **not**
include `.dll` files. Obtain runtimes from official sources only:

- **Wintun** — https://www.wintun.net/ (amd64 `wintun.dll` from the official zip;
  expected size 427552 bytes)
- **MinGW-w64 POSIX runtimes** — `libgcc_s_seh-1.dll`, `libstdc++-6.dll`,
  `libwinpthread-1.dll` from the official MinGW-w64 / distro mingw packages or
  MSYS2 (see [docs/RUN-WINDOWS.md](docs/RUN-WINDOWS.md))

## Bootstrap modes

`normal` (default), `local` (signed file only), and `chain3` (≥3 diverse RPC
seeds, 2-of-3 reconcile, height-lag cap, no live publisher fetch). Details:
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

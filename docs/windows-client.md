# Windows client

This repository starts from Session Foundation `session-router` **dev**
(imported as branch `main` here). It carries Windows client hardening.

**Do not open PRs from this tip into session-foundation** until an explicit public
flip is named.

## Upstream pin

Imported from: https://github.com/session-foundation/session-router  
Branch imported: `dev` (Foundation has no `main`; `dev` is the default tip)  
Exact SHA: `afb98f959f4f7e0ef996aef02fa21a6caf26b1e9`

## What this tip already locks (defaults)

Plain-language summary of packaged defaults. Details follow in later sections.

| Area | Packaged default | Why it matters |
|------|------------------|----------------|
| **Bootstrap** | `normal` (`fetch=true`); `local` and `chain3` available | How the client learns the relay set. See modes below. |
| **Local control API** | **off** | No unlocked JSON admin port. |
| **Bind (QUIC)** | `0.0.0.0` + ephemeral port `0` | Normal peer connectivity; not loopback-only. Fixed port on all interfaces needs explicit opt-in. |
| **Clearnet DNS** | **off** (NXDOMAIN for non-`.sesh` / `.snode`) | Avoids leaking ordinary DNS on your real IP by default. |
| **Reachable** | **false** | Client does not advertise itself as inbound-reachable. |
| **Exit** | **off** (`enable=false`) | Exit is not ready for general use. |
| **Auto exit routing** | **off** (`auto-routing=false`) | No automatic “send my internet via Exit”. |
| **Windows stay-up** | drain timer, IPv6 soft-fail scoped to TUN, libzstd linked | Reduces common MinGW/Windows drop and IPv6 foot-guns. |

Also see [RUN-WINDOWS.md](RUN-WINDOWS.md) for runtime DLLs (**not** bundled).

## Default vs developer / advanced paths

| Path | Who it is for | Double-click safe? |
|------|---------------|--------------------|
| Packaged defaults (`normal` bootstrap, API off, Exit off, DNS off, reachable false) | Everyday client use | **Yes** — intended safe baseline |
| `mode=local` | Operators who ship a known `bootstrap.signed` and want no live fetch | Only if the signed file is present and trusted |
| `mode=chain3` + `rpc=` seeds | Operators who deliberately run ≥3 diverse Oxen-style RPCs and understand quorum / lag caps | **No** — not a double-click path; misconfigured seeds refuse start or fall back only with a signed cold file |
| API enabled + `auth=` | Local tooling / debugging | Only on loopback/IPC with a strong secret |
| Exit / `auto-routing` enabled | Controlled tests only | **No** — Exit stays off in the package for a reason |

**chain3 is not the default and is not a “safer everyday” toggle.** It is an advanced bootstrap mode for people who can supply diverse RPC endpoints and read the failure modes.

## Bootstrap modes

1. **normal** (default) — `fetch=true` or omit, or `mode=normal`. Uses the signed
   bootstrap file if present, then may fetch a live relay list from the network.
2. **local** — `fetch=false` or `mode=local`. Uses only the signed bootstrap file in
   the install folder. Does **not** fetch a live list. If that file is missing or
   empty, Session Router refuses to start.
3. **chain3** — `mode=chain3`. Queries at least three diverse Oxen-style RPC
   endpoints (`rpc=`, repeatable), reconciles a 2-of-3 service-node view, applies a
   height-lag cap (`height-lag-cap=`, default 50), and does **not** perform live
   publisher fetch (`fetch=false` for this path). If RPCs fail and no signed
   `bootstrap.signed` cold fallback is present, startup is refused. Prefer distinct
   operators for the three RPCs; a same-operator trio is refused.

### Trust root: `bootstrap.signed`

The packaged `bootstrap.signed` file is a signed list of RelayContacts used to get
onto the network (and as cold fallback for `local` / `chain3`).

- Treat it as a **trust root**: if an attacker replaces it, they can bias who you
  first talk to.
- Prefer copies that ship with the official artifact for this tip, or that you
  built yourself from this repository at a known commit.
- `local` mode trusts **only** this file (no live fetch).
- `chain3` prefers a reconciled RPC view; the signed file is cold fallback only.

## Local control API

The local JSON control API is **off by default**.

If you turn it on:

- It may only listen on this computer (loopback) or a local ipc socket.
- You must set an `auth` secret in the config.
- Control commands need that secret. An open, unlocked admin port is not allowed.

## Network bind

The client UDP/QUIC socket binds **all interfaces** (`0.0.0.0`) with an
**ephemeral port** (port `0`). That is the normal default. Do **not** bind the
QUIC socket to `127.0.0.1` (that would break the client).

A **fixed** port on every address (for example `listen=:1191`) is opt-in only:
set `allow-all-interfaces=true`. Otherwise omit `listen` and keep the ephemeral
port.

## Clearnet DNS

By default this client does **not** look up normal internet names (anything that
is not `.sesh` or `.snode`) using your real IP.

Those lookups get NXDOMAIN until you deliberately set an upstream DNS server in
the config. Session names (`.sesh` / `.snode`) still work through Session Router.

## Reachable

By default this client does **not** publish itself as reachable on the network.

Other people are not invited to connect in to you unless you turn `reachable` on.

## Exit routing warning

Exit routing (sending your normal internet traffic out through a Session Router
exit) is **not ready for general use**.

The packaged client keeps Exit **off** (`[exit] enable=false`) and automatic exit
routing **off**. Do not turn these on for everyday use.

Deeper Exit work lives outside this Windows client tip; here Exit stays off.

### Exit security flaws

Reasons Exit stays off. Many are **protocol / client-routing class** (any platform
that enables Exit inherits them). A smaller set is **Windows-specific**.

**Protocol / client-routing class (not Windows-only):**

- Exit broker trust / injection — only an explicitly mapped exit may be treated as
  the exit broker.
- EXIT_CAPABLE gate — wait until the mapped exit is exit-capable before clearnet
  via Exit.
- Route bring-up fail-closed — no gateway / missing host pins must not partially
  succeed.
- Empty or bare default routes — empty ranges or bare `0.0.0.0/0` / `::/0` must
  not silently become full tunnel without explicit acknowledgement.
- Empty allow-all exit policy — `enable` with an empty policy must refuse, not
  allow all.
- API auth for privileged exit controls — map/swap/unmap need real admin auth.
- Unmap / empty ranges — last unmap must tear routes down.

**Windows-specific:**

- IPv6 binding scope — disable IPv6 on the Session Router TUN adapter only, never
  a global “all adapters” disable.
- Gateway / next-hop installation — Win32 next-hop selection and `route` errors
  must not install a bad default via the tunnel.

## Honest non-fixes (what this tip does not claim)

- **First hop still sees an IP.** Onion routing hides your destination from
  intermediate relays in the usual way; your network path to the first hop still
  has an address. That is a protocol fact, not a Windows bug.
- **Elevation.** Creating/using the TUN adapter on Windows typically needs
  appropriate privileges. This tip does not remove that OS requirement.
- **Upstream / Foundation limits.** Behavior inherited from upstream Session
  Router still applies; this tip hardens defaults and Windows packaging, it does
  not rewrite the whole protocol.
- **Exit depth.** Serious Exit hardening is tracked elsewhere. **This tip keeps
  Exit off** rather than pretending Exit is finished.
- **System VPN / commercial VPN under Session Router** (for example running a
  system-wide VPN beneath this client) is **out of scope for now** — future work,
  not documented as a supported setup here.

## Threat model (one pager)

### Protects / improves

- Default **off** for local admin API, clearnet DNS lookups, inbound
  “reachable”, and Exit / auto-routing.
- Honest bind wording: ephemeral port on all interfaces; no false claim of
  “localhost-only QUIC”.
- Bootstrap choices: signed-file-only (`local`) and optional multi-RPC reconcile
  (`chain3`) for operators who need them.
- Windows stay-up hardening (drain / IPv6 soft-fail / zstd) aimed at fewer silent
  client drops.
- Supply chain for the **published artifact**: exe + config/docs only; runtimes
  from official vendors you can verify.

### Does not protect

- A compromised or replaced `bootstrap.signed` (trust root).
- A malicious or colluding first hop’s view of your IP on the path to it.
- Endpoint malware, OS compromise, or a bad MinGW/Wintun DLL dropped next to the
  exe from an unofficial source.
- Full clearnet anonymity via Exit (Exit is off; even when on, Exit has known
  flaws listed above).
- Guarantees against global network observers or targeted traffic-correlation
  attacks beyond what the Session Router protocol itself provides.

## Logging

Packaged logging defaults to a local file (`session-router.log` in the working
directory when using the packaged ini). Logs can contain operational detail
(paths, peers, errors). Treat log files as sensitive on shared machines; do not
publish them with session keys or auth secrets.

## Authenticity: commit and build

When you care about provenance:

1. **Source tip** — note the git commit SHA of this repository (for example
   `git rev-parse HEAD`). Prefer building or downloading an artifact that names
   that SHA.
2. **CI artifact naming** — the `windows-source` workflow uploads
   `session-router-windows-source-<SHA>/` containing the exe and docs for that
   commit. Match the SHA in the artifact name to the commit you intend.
3. **Binary** — after download, record a hash locally (for example
   `Get-FileHash session-router.exe` on Windows) and keep it with the commit SHA.
4. **Runtimes** — verify Wintun size/PE machine type and take MinGW DLLs from the
   official toolchain that matches the build (see [RUN-WINDOWS.md](RUN-WINDOWS.md)).
   Do not mix random DLLs from third-party “all-in-one” zips.

## Supply chain: no bundled DLLs

Published artifacts intentionally **omit** `.dll` files. Obtain:

- **Wintun** from https://www.wintun.net/ (amd64 DLL, expected **427552** bytes)
- **MinGW-w64 POSIX** `libgcc_s_seh-1.dll`, `libstdc++-6.dll`, `libwinpthread-1.dll`
  from official mingw-w64 / distro packages or MSYS2

Full steps: [RUN-WINDOWS.md](RUN-WINDOWS.md).

## Defaults summary (quick table)

| Setting | Default |
|---------|---------|
| Bootstrap | normal (`fetch=true`); local and chain3 available (advanced) |
| Local API | off |
| Bind | `0.0.0.0` + ephemeral port `0`; fixed port needs `allow-all-interfaces=true` |
| Clearnet DNS | off (NXDOMAIN for non-.sesh/.snode) |
| Reachable | false |
| Exit | off |
| Auto exit routing | off |
| Artifact DLLs | none (official sources only) |

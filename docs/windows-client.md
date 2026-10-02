# Windows client (security-oriented fork)

This repository is a **security-oriented fork** of Session Foundation
`session-router`, imported from upstream branch `dev` as this repository’s
`main`. Its identity is **security defaults and Exit honesty**; Windows remains
the primary build and run target in this guide. It hardens client defaults,
packaging, and docs for people who want a usable Windows build without upstream
project internals.

This is a fork. Contribution or pull requests into Session Foundation are **not**
the goal of this repository.

## Upstream pin

Imported from: https://github.com/session-foundation/session-router  
Branch imported: `dev` (Foundation’s default development branch; there is no
`main` there)  
Exact SHA: `afb98f959f4f7e0ef996aef02fa21a6caf26b1e9`

## What the default build already locks

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

Also see [RUN-WINDOWS.md](RUN-WINDOWS.md) for runtime DLLs (**not** bundled) and the **required** tunnel DNS step for `.sesh` names (`netsh ... sr-tun0 ... 127.0.0.1`, or `tunnel-dns-preflight.ps1`).

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
- Prefer copies that ship with the official artifact for this repository, or that
  you built yourself from a known commit.
- `local` mode trusts **only** this file (no live fetch).
- `chain3` prefers a reconciled RPC view; the signed file is cold fallback only.

## Local control API

The local JSON control API is **off by default**.

If you turn it on:

- It may only listen on this computer (loopback) or a local ipc socket.
- You must set an `auth` secret in the config.
- Control commands need that secret. An open, unlocked admin port is not allowed.

## Network bind (lesson)

The client UDP/QUIC socket binds **all interfaces** (`0.0.0.0`) with an
**ephemeral port** (port `0`). That is the normal default for peer connectivity.

**Do not** bind the QUIC socket to `127.0.0.1`. That would break the client.
Loopback is appropriate for the local API and local DNS listener — not for the
onion UDP path.

### What upstream packaging/docs left confused

Two different problems got tangled together:

1. **Fixed port on all interfaces** — shipping something like `listen=:1191`
   puts a stable UDP listener on every address. That is a wider local exposure
   than most desktop users intend, and easy to misread as “the one VPN port.”
2. **“Just bind loopback”** — a tempting-sounding mitigation that is simply
   wrong for QUIC. Ephemeral-port-on-`0.0.0.0` is not the same thing as
   localhost-only. Treating them as synonyms left civilians either exposed or
   broken depending on which bad advice they followed.

### What this fork changed

- Packaged config **omits** a fixed `listen=:PORT`. Default remains
  all-interfaces + ephemeral port `0`.
- Code **refuses** `listen=:PORT` on all interfaces unless
  `allow-all-interfaces=true`.
- Docs state the truth: we did **not** kill the public QUIC socket; we locked a
  safer listen default and corrected the wording.

A **fixed** port on every address remains opt-in only: set
`allow-all-interfaces=true`. Otherwise omit `listen` and keep the ephemeral
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

Upstream left Exit available as a configuration surface without shipping an
equally serious account of what still fails when it is enabled. That gap —
defaults and docs that a civilian can walk past — is why this fork treats Exit
as a documented hazard with the switch left off, not as a finished product
feature.

### Exit / client-routing flaw status

**Skimmer note:** rows below are *not* automatically “unfixed in the default
build.” Several serious items are **mitigated by Exit (and auto-routing) staying
off**. “Still open if Exit enabled” means the underlying protocol / client issue
remains if an operator turns Exit on — it does **not** mean the shipped default
exposes that path.

| Flaw | Scope | Status in this fork |
|------|-------|---------------------|
| Broker trust / injection | Protocol / client-routing | Mitigated by Exit-off (and/or auto-routing off); Still open if Exit enabled |
| EXIT_CAPABLE gate | Protocol / client-routing | Mitigated by Exit-off (and/or auto-routing off); Still open if Exit enabled |
| Fail-closed bring-up (missing gateway / host pins) | Protocol / client-routing | Mitigated by Exit-off (and/or auto-routing off); Still open if Exit enabled |
| Empty ranges / bare `0.0.0.0/0` / `::/0` (full tunnel) | Protocol / client-routing | Mitigated by Exit-off (and/or auto-routing off); Still open if Exit enabled |
| `enable` + empty policy behaves as allow-all | Protocol / client-routing | Mitigated by Exit-off (and/or auto-routing off); Still open if Exit enabled |
| Last unmap leaves routes | Protocol / client-routing | Mitigated by Exit-off (and/or auto-routing off); Still open if Exit enabled |
| Map / swap / unmap without real API credentials | Local API / Exit controls | Fixed in this fork (code/docs) when API used (API off by default; if on need `auth=`) |
| IPv6 disable / soft-fail hits all adapters | Windows-specific | Fixed in this fork (code/docs) — TUN-scoped |
| Bad Win32 gateway / next-hop | Windows-specific + Exit path | Mitigated by Exit-off / auto-routing off; Still open if Exit + auto-routing on |

**Defaults around Exit (not an Exit-on product claim):** packaged client keeps
`[exit] enable=false` and `auto-routing=false`. Clearnet DNS (`upstream=`) and
`reachable` stay **off** by default so a normal install does not widen exposure
beside the Exit switch. Enabling Exit yourself accepts the “Still open if Exit
enabled” rows above.

## Honest non-fixes (what this fork does not claim)

- **First hop still sees an IP.** Onion routing hides your destination from
  intermediate relays in the usual way; your network path to the first hop still
  has an address. That is a protocol fact, not a Windows bug.
- **Elevation.** Creating/using the TUN adapter on Windows typically needs
  appropriate privileges. This fork does not remove that OS requirement.
- **Upstream limits.** Behavior inherited from upstream Session Router still
  applies; this fork hardens defaults and Windows packaging, it does not rewrite
  the whole protocol.
- **Exit depth.** Serious Exit hardening is unfinished. **This fork keeps Exit
  off** rather than pretending Exit is finished.
- **System VPN / commercial VPN under Session Router** (for example running a
  system-wide VPN beneath this client) is **out of scope for now** — future work,
  not documented as a supported setup here.

## Threat model (one pager)

### Protects / improves

- Default **off** for local admin API, clearnet DNS lookups, inbound
  “reachable”, and Exit / auto-routing.
- Honest bind wording: ephemeral port on all interfaces; no false claim of
  “localhost-only QUIC”; fixed all-interfaces port refused without opt-in.
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

1. **Source commit** — note the git commit SHA of this repository (for example
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

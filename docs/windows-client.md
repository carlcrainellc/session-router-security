# Windows client

This repository starts from Session Foundation `session-router` **dev**
(imported as branch `main` here). It carries Windows client hardening.

**Do not open PRs from this tip into session-foundation** until an explicit public flip is named.

## Upstream pin

Imported from: https://github.com/session-foundation/session-router  
Branch imported: `dev` (Foundation has no `main`; `dev` is the default tip)  
Exact SHA: `afb98f959f4f7e0ef996aef02fa21a6caf26b1e9`

## What this client does by default (plain language)


## Local control API

The local JSON control API is **off by default**.

If you turn it on:

- It may only listen on this computer (loopback) or a local ipc socket.
- You must set an `auth` secret in the config.
- Control commands need that secret. An open, unlocked admin port is not allowed.

## Bootstrap modes

Three modes:

1. **normal** (default) — `fetch=true` or omit, or `mode=normal`. Uses the signed bootstrap
   file if present, then may fetch a live relay list from the network.
2. **local** — `fetch=false` or `mode=local`. Uses only the signed bootstrap file in the
   install folder. Does **not** fetch a live list. If that file is missing or empty, Session
   Router refuses to start.
3. **chain3** — `mode=chain3`. Queries at least three diverse Oxen-style RPC endpoints
   (`rpc=`, repeatable), reconciles a 2-of-3 service-node view, applies a height-lag cap
   (`height-lag-cap=`, default 50), and does **not** perform live publisher fetch
   (`fetch=false` for this path). If RPCs fail and no signed `bootstrap.signed` cold
   fallback is present, startup is refused. Prefer distinct operators for the three RPCs;
   a same-operator trio is refused.

Runtime DLLs are **not** shipped with the build artifact. See [RUN-WINDOWS.md](RUN-WINDOWS.md).


## Exit routing warning

Exit routing (sending your normal internet traffic out through a Session Router exit)
is **not ready for general use**.

The packaged client keeps Exit **off** (`[exit] enable=false`) and automatic exit
routing **off**. Do not turn these on for everyday use. Only use Exit in a private
test if you understand the risk.

## Network bind

The client UDP/QUIC socket binds **all interfaces** (`0.0.0.0`) with an **ephemeral port**
(port `0`). That is the normal default. Do **not** bind the QUIC socket to `127.0.0.1`
(that would break the client).

A **fixed** port on every address (for example `listen=:1191`) is opt-in only:
set `allow-all-interfaces=true`. Otherwise omit `listen` and keep the ephemeral port.

## Clearnet DNS

By default this client does **not** look up normal internet names (anything that is not
`.sesh` or `.snode`) using your real IP.

Those lookups get NXDOMAIN until you deliberately set an upstream DNS server in the
config. Session names (`.sesh` / `.snode`) still work through Session Router.

## Reachable

By default this client does **not** publish itself as reachable on the network.

Other people are not invited to connect in to you unless you turn `reachable` on.

## Defaults summary (this tip)

| Setting | Default |
|---------|---------|
| Bootstrap | normal (`fetch=true`); local and chain3 available |
| Local API | off |
| Bind | `0.0.0.0` + ephemeral port `0`; fixed port needs `allow-all-interfaces=true` |
| Clearnet DNS | off (NXDOMAIN for non-.sesh/.snode) |
| Reachable | false |
| Exit | off |
| Auto exit routing | off |

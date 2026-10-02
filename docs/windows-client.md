# Windows client (private tip)

This private repository starts from Session Foundation `session-router` **dev**
(imported as branch `main` here). It is for Windows client hardening review only.

**Do not open PRs from this tip into session-foundation.**

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

Two modes (no third mode in this tip):

1. **normal** (default) — `fetch=true` or omit, or `mode=normal`. Uses the signed bootstrap
   file if present, then may fetch a live relay list from the network.
2. **local** — `fetch=false` or `mode=local`. Uses only the signed bootstrap file in the
   install folder. Does **not** fetch a live list. If that file is missing or empty, Session
   Router refuses to start.


## Exit routing warning

Exit routing (sending your normal internet traffic out through a Session Router exit)
is **not ready for general use**. Keep Exit off unless you are doing a controlled
private test and know exactly what you are turning on.

## Network bind

The packaged client does **not** open fixed port 1191 on every network card.

By default it uses a temporary port. Do not set `listen=:1191` unless you know you need
it and you set `allow-all-interfaces=true` on purpose.

## Clearnet DNS

By default this client does **not** look up normal internet names (anything that is not
`.sesh` or `.snode`) using your real IP.

Those lookups get NXDOMAIN until you deliberately set an upstream DNS server in the
config. Session names (`.sesh` / `.snode`) still work through Session Router.

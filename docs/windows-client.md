# Windows client (private tip)

This private repository starts from Session Foundation `session-router` **dev**
(imported as branch `main` here). It is for Windows client hardening review only.

**Do not open PRs from this tip into session-foundation.**

## Upstream pin

Imported from: https://github.com/session-foundation/session-router  
Branch imported: `dev` (Foundation has no `main`; `dev` is the default tip)  
Exact SHA: `afb98f959f4f7e0ef996aef02fa21a6caf26b1e9`

## What this client does by default (plain language)

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

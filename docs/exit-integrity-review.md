# Exit integrity review (this branch)

Branch: `private/exit-integrity`  
Target: `main` on [carlcrainellc/session-router-security](https://github.com/carlcrainellc/session-router-security)  
Purpose: review pack for client Exit integrity fixes. **Exit stays off by default**
in the packaged client (`[exit] enable=false`, `auto-routing=false`) until review
says otherwise.

This is **not** an Exit-on release claim. The packaged defaults, API-off + `auth=`
gate, clearnet DNS off, and `reachable=false` from `main` are preserved.

## What changed (code)

Ported client Exit integrity work onto this security fork:

- Map / list / unmap / swap Exit RPCs that actually map ranges, apply tokens, and
  drive route bring-up / tear-down (with explicit `full_tunnel` for empty or bare
  `0.0.0.0/0` / `::/0`).
- Broker check is map-only (no treating an arbitrary Exit-policy peer as a broker).
- Outbound Exit traffic waits until the mapped session is `EXIT_CAPABLE`.
- Route poker fail-closed without a discovered non-TUN gateway or host pins.
- Unmap of the last ranges calls route poker down.
- `[exit] enable=true` with an empty policy / no routed-range refuses startup
  (and packet allow stays deny-all without a policy).
- Win32 default route uses the TUN network address as next hop; IPv6 disable stays
  TUN-scoped (already on `main`, kept here).
- Privileged Exit RPCs use this fork’s existing API gate: API off by default; if
  enabled, loopback/IPC bind + matching `[api] auth=` (same as other privileged
  commands).

## Status vs prior “mitigated by Exit-off” table

| Item | Status on this branch |
|------|------------------------|
| Broker trust / injection | **FIXED** (code) |
| EXIT_CAPABLE wait | **FIXED** (code) |
| put_up fail-closed (gateway / host pins) | **FIXED** (code) |
| Unmap last ranges → put_down | **FIXED** (code) |
| MapExit / SwapExits → put_up when ranges set | **FIXED** (code) |
| MapExit API credentials | **FIXED** (this fork’s `auth=` gate; API off by default) |
| Empty / bare `0.0.0.0/0` needs `full_tunnel=true` | **FIXED** (code) |
| Win32 IPv6 TUN-scoped | **FIXED** (already on `main`) |
| Win32 next-hop via TUN network address | **FIXED** (code) |
| `[exit] enable` + empty policy fail-closed | **FIXED** (code) |
| Overlapping ranges / token into live map / parse-then-replace | **FIXED** (code) |
| OMQ vs packet-path data race | **OPEN** (MapExit on job queue; packet path still reads ranges without a shared mutex) |
| DNS on-link leak (`set_dns_mode`) | **NOTED** (still not wired) |

Packaged Exit remains **`enable=false`** / **`auto-routing=false`**. Code fixes do
not flip those defaults.

## Docs

- README and [windows-client.md](windows-client.md) Exit status tables mark the
  code-fixed rows accordingly and keep the “off by default for review” note.
- [RUN-WINDOWS.md](RUN-WINDOWS.md) still tells operators to leave Exit off for
  everyday use.

## Review ask

Confirm the integrity fixes, keep Exit off in packaging until explicitly approved,
and do not treat this branch as an Exit-on product ship.

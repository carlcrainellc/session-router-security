# Exit integrity notes

This fork’s client Exit path: what was wrong upstream, what is fixed in code
here, and what remains open.

**Packaged Exit stays off** (`[exit] enable=false`, `auto-routing=false`). That
is intentional. This page is not an Exit-on product claim.

Safer client defaults elsewhere on this fork (API off + `auth=` gate, clearnet
DNS off, `reachable=false`) are unchanged.

## What was wrong upstream

When Exit is enabled, upstream client behavior left several integrity holes that
a config toggle does not advertise:

- **Broker trust:** return traffic could be accepted from a peer that merely
  advertised Exit policy, not only from the Exit that was actually mapped.
- **`EXIT_CAPABLE`:** outbound clearnet via Exit could start before the mapped
  session was ready.
- **Route bring-up:** default routes via the TUN could be installed without a
  discovered non-TUN gateway or host pins (fail-open).
- **Silent full-tunnel:** empty `ip_ranges` or bare `0.0.0.0/0` / `::/0` could
  map without an explicit `full_tunnel` flag.
- **Empty policy:** `[exit] enable=true` with no routed-range / policy could
  behave as allow-all.
- **Unmap:** removing the last mapped ranges did not tear routes down.
- **Privileged RPCs:** map / swap / unmap were not gated by this fork’s real API
  auth model (API off by default; if on, matching `[api] auth=`).
- **Windows:** IPv6 disable could hit all adapters; default next-hop via the
  tunnel used an unreliable address trick.

These are protocol / client-routing issues. They are not Windows-only. Official
Foundation builds can still carry them if Exit is enabled.

## What this fork fixed in code

| Item | Status |
|------|--------|
| Only mapped Exit injects return traffic (map-only broker check) | **Fixed (code)** |
| Wait for `EXIT_CAPABLE` before clearnet via Exit | **Fixed (code)** |
| Fail-closed route bring-up without gateway / host pins | **Fixed (code)** |
| MapExit / SwapExits drive route bring-up when ranges are set | **Fixed (code)** |
| Empty / bare `0.0.0.0/0` / `::/0` requires `full_tunnel=true` | **Fixed (code)** |
| `[exit] enable` + empty policy refuses startup (deny-all without policy) | **Fixed (code)** |
| Last unmap tears routes down | **Fixed (code)** |
| Overlapping ranges / token into live map / parse-then-replace | **Fixed (code)** |
| Privileged Exit RPCs require this fork’s `[api] auth=` when API is on | **Fixed (code)** |
| Win32 IPv6 disable scoped to the TUN adapter | **Fixed (code)** (also on `main`) |
| Win32 default next-hop uses TUN network address | **Fixed (code)** |

## Still open

| Item | Status |
|------|--------|
| OMQ vs packet-path data race (live Exit ranges read without a shared mutex) | **Still open** |
| DNS on-link leak (`set_dns_mode` still not wired) | **Still open** |

## Defaults (not an Exit-on ship)

| Setting | Packaged value |
|---------|----------------|
| `[exit] enable` | `false` |
| `auto-routing` | `false` |

Code fixes do not flip those defaults. Clearnet DNS (`upstream=`) and
`reachable` also stay off by default so a normal install does not widen
exposure beside the Exit switch.

## Behavior of the fixed paths

With Exit enabled for testing only, the fixed client paths refuse unsafe maps
and tear routes down on last unmap:

- Empty `ip_ranges` without `full_tunnel=true` → refused.
- Bare `0.0.0.0/0` / `::/0` without `full_tunnel=true` → refused.
- Unmap of the last ranges → route poker tear-down.

Everyday use should leave Exit off. See also the README status table and
[windows-client.md](windows-client.md).

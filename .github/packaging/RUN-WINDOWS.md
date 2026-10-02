# Windows package — run steps

Release zip: statically linked `session-router.exe` + official `wintun.dll` only (no MinGW runtime DLLs required). Full guide: [docs/RUN-WINDOWS.md](../../docs/RUN-WINDOWS.md).

1. Keep Exit disabled in the client config (`enable=false` under `[exit]`).
2. Start `session-router.exe`. The tunnel adapter `sr-tun0` appears after a few seconds.
3. **Required for `.sesh` names:** set tunnel DNS to the local Session Router
   resolver. Without this step, Session Router names (`.sesh` / `.snode`) will
   **not** resolve — Windows will not send those queries to the client. The zip
   does **not** run this for you:

```
netsh interface ip set dns name="sr-tun0" static 127.0.0.1 primary validate=no
```

Or run the optional pre-flight script from this folder (same steps: wait for
`sr-tun0`, set DNS, show DNS):

```
powershell -ExecutionPolicy Bypass -File tunnel-dns-preflight.ps1
```

4. Verify:

```
netsh interface ip show dns name="sr-tun0"
```

Expected: statically configured DNS server `127.0.0.1`.

See also the full client guide: [windows-client.md](../../docs/windows-client.md)
and [docs/RUN-WINDOWS.md](../../docs/RUN-WINDOWS.md).

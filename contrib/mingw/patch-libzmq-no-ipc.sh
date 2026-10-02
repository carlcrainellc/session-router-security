#!/usr/bin/env bash
# Disable libzmq IPC for MinGW cross builds (avoids Unix sys/socket.h).
set -euo pipefail
root="$(cd "$(dirname "$1")" && pwd)"
cmake_lists="$1"
test -f "$cmake_lists"
python3 - "$cmake_lists" <<'PY'
import sys
from pathlib import Path
p = Path(sys.argv[1])
t = p.read_text()
t = t.replace("set(ZMQ_HAVE_IPC 1)", "set(ZMQ_HAVE_IPC 0)")
needle = '  check_include_files("winsock2.h;afunix.h" ZMQ_HAVE_IPC)\n'
force = needle + "  set(ZMQ_HAVE_IPC OFF)\n"
if needle in t and force not in t:
    t = t.replace(needle, force, 1)
# Also neutralize unconditional IPC sources for MinGW: wrap is hard; instead
# replace ipc_*.cpp bodies' socket includes via empty stubs after extract.
p.write_text(t)
print(f"patched {p} CMakeLists IPC off")
PY
# Patch ipc_*.cpp/hpp that pull sys/socket.h on non-MSVC
cd "$root"
for f in src/ipc_address.hpp src/ipc_connecter.cpp src/ipc_listener.cpp; do
  if [ -f "$f" ]; then
    # Prefer afunix/winsock on MinGW; or empty-out the Unix includes
    sed -i 's/#include <sys\/socket.h>/#if defined(__MINGW32__)\n#include <winsock2.h>\n#include <afunix.h>\n#else\n#include <sys\/socket.h>\n#endif/' "$f" || true
    sed -i 's/#include <sys\/un.h>/#if !defined(__MINGW32__)\n#include <sys\/un.h>\n#endif/' "$f" || true
    sed -i 's/#if defined _MSC_VER/#if defined(_MSC_VER) || defined(__MINGW32__)/' "$f" || true
  fi
done
echo "patched IPC sources under $root"

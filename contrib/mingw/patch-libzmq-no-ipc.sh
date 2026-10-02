#!/usr/bin/env bash
# Disable libzmq IPC for MinGW cross builds (avoids Unix sys/socket.h).
set -euo pipefail
cmake_lists="${1:-CMakeLists.txt}"
test -f "$cmake_lists"
# Unix branch default
sed -i 's/set(ZMQ_HAVE_IPC 1)/set(ZMQ_HAVE_IPC 0)/' "$cmake_lists"
# Windows afunix probe can enable IPC; MinGW still compiles Unix includes — force off.
if ! grep -q 'set(ZMQ_HAVE_IPC OFF)' "$cmake_lists"; then
  sed -i '/check_include_files("winsock2.h;afunix.h" ZMQ_HAVE_IPC)/a\  set(ZMQ_HAVE_IPC OFF)' "$cmake_lists"
fi
# Belt: ensure ipc_address.hpp never pulls sys/socket.h on MinGW even if IPC slips through
if [ -f src/ipc_address.hpp ]; then
  sed -i 's/#if defined _MSC_VER/#if defined(_MSC_VER) || defined(__MINGW32__)/' src/ipc_address.hpp
fi
echo "patched $cmake_lists for no-IPC MinGW"

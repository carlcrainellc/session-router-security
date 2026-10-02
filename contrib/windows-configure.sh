#!/bin/bash
set -e
set -x

# Usage: windows-configure.sh [rootdir [builddir]] -DWHATEVER=BLAH ...

if [ $# -ge 1 ] && [[ "$1" != -* ]]; then
    root="$1"
    shift
else
    root="$(dirname $0)"/..
fi
root="$(readlink -f "$root")"

if [ $# -ge 1 ] && [[ "$1" != -* ]]; then
    build="$(readlink -f "$1")"
    shift
else
    build="$root/build/win32"
    echo "Setting up build in $build"
fi

mkdir -p "$build"

# MinGW: use tip-local libzmq ExternalProject recipe (IPC off + toolchain order).
if [ -f "$root/contrib/mingw/LocalLibzmq.cmake" ]; then
  cp -v "$root/contrib/mingw/LocalLibzmq.cmake"     "$root/external/oxen-mq/cmake/local-libzmq/LocalLibzmq.cmake"
fi

# MinGW/GCC14: libunbound ub_event callbacks use int fd; libevent wants intptr_t.
if [ -f "$root/cmake/session-deps/deps/libunbound.cmake" ]; then
  sed -i 's/-D_WIN32_WINNT=0x0600"/-D_WIN32_WINNT=0x0600 -Wno-incompatible-pointer-types"/'     "$root/cmake/session-deps/deps/libunbound.cmake" || true
fi

cmake \
    -S "$root" -B "$build" \
    -G 'Unix Makefiles' \
    -DCMAKE_EXE_LINKER_FLAGS=-fstack-protector \
    -DCMAKE_CXX_FLAGS=-fdiagnostics-color=always \
    -DCMAKE_TOOLCHAIN_FILE="$root/contrib/cross/mingw64.cmake" \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_STATIC_DEPS=ON \
    -DSROUTER_PACKAGE=ON \
    -DBUILD_SHARED_LIBS=OFF \
    -DBUILD_TESTING=OFF \
    -DSROUTER_TESTS=OFF \
    -DSROUTER_BOOTSTRAP=OFF \
    -DSROUTER_NATIVE_BUILD=OFF \
    -DSTATIC_LINK=ON \
    -DWITH_SYSTEMD=OFF \
    -DFORCE_OXENMQ_SUBMODULE=ON \
    -DFORCE_OXENC_SUBMODULE=ON \
    -DFORCE_FMT_SUBMODULE=ON \
    -DFORCE_SPDLOG_SUBMODULE=ON \
    -DFORCE_NLOHMANN_SUBMODULE=ON \
    -DWITH_LTO=OFF \
    "$@"

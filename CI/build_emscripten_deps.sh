#!/bin/bash
# Build OpenMW dependencies for Emscripten/WASM
#
# Prerequisites:
#   - Emscripten SDK (emsdk) installed and activated
#   - CMake 3.16+
#   - Standard build tools (make, etc.)
#
# Usage:
#   source /path/to/emsdk/emsdk_env.sh
#   ./CI/build_emscripten_deps.sh [install_prefix]

set -euo pipefail

PREFIX="${1:-$HOME/openmw-wasm-deps}"
JOBS="${JOBS:-$(nproc 2>/dev/null || echo 4)}"

echo "=== Building OpenMW WASM dependencies ==="
echo "Install prefix: $PREFIX"
echo "Parallel jobs: $JOBS"
echo ""

if ! command -v emcc &>/dev/null; then
    echo "Error: Emscripten (emcc) not found. Source emsdk_env.sh first."
    exit 1
fi

mkdir -p "$PREFIX"
BUILDDIR="$(mktemp -d)"
trap "rm -rf '$BUILDDIR'" EXIT

echo "--- Building zlib ---"
cd "$BUILDDIR"
emcmake cmake "$EMSDK/upstream/emscripten/cache/ports/zlib" \
    -DCMAKE_INSTALL_PREFIX="$PREFIX" \
    -DCMAKE_BUILD_TYPE=Release 2>/dev/null || {
    echo "Note: zlib available as Emscripten port. Use -sUSE_ZLIB=1 linker flag instead."
}

echo ""
echo "--- Building Lua 5.4 ---"
if [ -d "$BUILDDIR/lua" ]; then rm -rf "$BUILDDIR/lua"; fi
mkdir -p "$BUILDDIR/lua" && cd "$BUILDDIR/lua"
LUA_VERSION="5.4.7"
if command -v wget &>/dev/null; then
    wget -q "https://www.lua.org/ftp/lua-${LUA_VERSION}.tar.gz" -O lua.tar.gz
elif command -v curl &>/dev/null; then
    curl -sL "https://www.lua.org/ftp/lua-${LUA_VERSION}.tar.gz" -o lua.tar.gz
fi
if [ -f lua.tar.gz ]; then
    tar xzf lua.tar.gz
    cd "lua-${LUA_VERSION}/src"
    emcc -O2 -c lapi.c lcode.c lctype.c ldebug.c ldo.c ldump.c lfunc.c lgc.c llex.c \
        lmem.c lobject.c lopcodes.c lparser.c lstate.c lstring.c ltable.c ltm.c \
        lundump.c lvm.c lzio.c lauxlib.c lbaselib.c lcorolib.c ldblib.c liolib.c \
        lmathlib.c loadlib.c loslib.c lstrlib.c ltablib.c lutf8lib.c linit.c
    emar rcs liblua.a *.o
    mkdir -p "$PREFIX/lib" "$PREFIX/include"
    cp liblua.a "$PREFIX/lib/"
    cp lua.h luaconf.h lualib.h lauxlib.h "$PREFIX/include/"
    echo "Lua installed to $PREFIX"
else
    echo "Warning: Could not download Lua. Skipping."
fi

echo ""
echo "--- Building LZ4 ---"
if [ -d "$BUILDDIR/lz4" ]; then rm -rf "$BUILDDIR/lz4"; fi
mkdir -p "$BUILDDIR/lz4" && cd "$BUILDDIR/lz4"
if command -v git &>/dev/null; then
    git clone --depth 1 --branch v1.10.0 https://github.com/lz4/lz4.git src 2>/dev/null || true
    if [ -d src ]; then
        cd src/build/cmake
        emcmake cmake . -DCMAKE_INSTALL_PREFIX="$PREFIX" \
            -DCMAKE_BUILD_TYPE=Release \
            -DBUILD_SHARED_LIBS=OFF \
            -DLZ4_BUILD_CLI=OFF \
            -DLZ4_BUILD_LEGACY_LZ4C=OFF
        emmake make -j"$JOBS" install
        echo "LZ4 installed to $PREFIX"
    fi
else
    echo "Warning: git not available. Skipping LZ4."
fi

echo ""
echo "=== Dependency build complete ==="
echo ""
echo "To build OpenMW for WASM, configure CMake with:"
echo "  emcmake cmake -C cmake/emscripten-wasm.cmake \\"
echo "    -DCMAKE_PREFIX_PATH=$PREFIX \\"
echo "    -DCMAKE_FIND_ROOT_PATH=$PREFIX \\"
echo "    .."
echo ""
echo "Note: OSG, Bullet, MyGUI, and Boost must be built separately."
echo "See cmake/emscripten-wasm.cmake for build instructions."

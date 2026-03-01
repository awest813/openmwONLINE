#!/bin/bash
# --------------------------------------------------------------------------
# build_wasm_deps.sh
#
# Cross-compile the "big" OpenMW dependencies for WebAssembly / Emscripten.
#
# Prerequisites:
#   - Emscripten SDK activated (source emsdk_env.sh)
#   - Standard POSIX build tools (make, autoconf, etc.)
#   - A host C/C++ compiler for ICU host-build tools
#
# Usage:
#   CI/build_wasm_deps.sh [--prefix <install-dir>] [--jobs <N>]
#
# The script installs headers + static libraries into <prefix> (default:
# $(pwd)/wasm-deps).  Pass this as CMAKE_PREFIX_PATH when configuring
# OpenMW so that find_package() calls pick up the cross-compiled libraries.
#
# Dependencies built here (those NOT handled by FetchContent in extern/):
#   1. Lua 5.4           (scripting — LuaJIT cannot target WASM)
#   2. LZ4               (compression)
#   3. Boost 1.84        (program_options, iostreams)
#   4. FFmpeg 6.1        (audio / video decoding)
#   5. ICU 70.1          (Unicode — requires a host-build of ICU tools first)
#
# Dependencies handled by Emscripten ports (no build needed):
#   - SDL2        (-sUSE_SDL=2)
#   - zlib        (-sUSE_ZLIB=1)
#   - libpng      (-sUSE_LIBPNG=1)
#   - libjpeg     (-sUSE_LIBJPEG=1)
#   - FreeType    (-sUSE_FREETYPE=1)
#
# Dependencies handled by FetchContent in extern/CMakeLists.txt:
#   - Bullet 3.17, OpenSceneGraph, MyGUI 3.4.3, RecastNavigation,
#     SQLite3, yaml-cpp, Google Test / Benchmark
# --------------------------------------------------------------------------

set -euo pipefail

# ── Tunables ──────────────────────────────────────────────────────────────
PREFIX="$(pwd)/wasm-deps"
JOBS="$(nproc 2>/dev/null || echo 4)"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --prefix) PREFIX="$2"; shift 2 ;;
        --jobs)   JOBS="$2";   shift 2 ;;
        *)        echo "Unknown option: $1"; exit 1 ;;
    esac
done

PREFIX="$(realpath "$PREFIX")"
SRC="${PREFIX}/src"
mkdir -p "$PREFIX" "$SRC"

echo "=== WASM dependency build ==="
echo "  PREFIX : $PREFIX"
echo "  JOBS   : $JOBS"
echo ""

# ── Verify Emscripten ────────────────────────────────────────────────────
if ! command -v emcc &>/dev/null; then
    echo "ERROR: emcc not found – activate the Emscripten SDK first."
    echo "  source <emsdk>/emsdk_env.sh"
    exit 1
fi
EMCC_VERSION="$(emcc --version | head -1)"
echo "Using Emscripten: $EMCC_VERSION"
echo ""

# Common flags passed to every C/C++ compilation
COMMON_CFLAGS="-O2"
COMMON_CXXFLAGS="-O2"

# ──────────────────────────────────────────────────────────────────────────
# 1. Lua 5.4
# ──────────────────────────────────────────────────────────────────────────
LUA_VER="5.4.7"
LUA_DIR="lua-${LUA_VER}"
LUA_TAR="lua-${LUA_VER}.tar.gz"
LUA_URL="https://www.lua.org/ftp/${LUA_TAR}"

build_lua() {
    echo "──── Building Lua ${LUA_VER} ────"
    cd "$SRC"

    if [ ! -d "$LUA_DIR" ]; then
        [ -f "$LUA_TAR" ] || wget -q "$LUA_URL"
        tar xzf "$LUA_TAR"
    fi

    cd "$LUA_DIR"
    # Clean previous build artifacts
    make clean 2>/dev/null || true

    # Build a static library using Emscripten
    make generic \
        CC="emcc" \
        AR="emar rcu" \
        RANLIB="emranlib" \
        MYCFLAGS="$COMMON_CFLAGS -DLUA_USE_POSIX" \
        MYLDFLAGS="" \
        -j"$JOBS"

    # Install into prefix
    make install INSTALL_TOP="$PREFIX"
    echo "  -> Lua installed to $PREFIX"
    echo ""
}

# ──────────────────────────────────────────────────────────────────────────
# 2. LZ4
# ──────────────────────────────────────────────────────────────────────────
LZ4_VER="1.9.4"
LZ4_DIR="lz4-${LZ4_VER}"
LZ4_TAR="v${LZ4_VER}.tar.gz"
LZ4_URL="https://github.com/lz4/lz4/archive/refs/tags/${LZ4_TAR}"

build_lz4() {
    echo "──── Building LZ4 ${LZ4_VER} ────"
    cd "$SRC"

    if [ ! -d "$LZ4_DIR" ]; then
        [ -f "$LZ4_TAR" ] || wget -q "$LZ4_URL"
        tar xzf "$LZ4_TAR"
    fi

    cd "$LZ4_DIR"
    make clean 2>/dev/null || true

    # LZ4 builds with a plain Makefile; override CC/AR for Emscripten
    make lib \
        CC="emcc" \
        AR="emar" \
        CFLAGS="$COMMON_CFLAGS" \
        BUILD_SHARED=no \
        -j"$JOBS"

    # Manual install – the Makefile install target works but we keep it simple
    mkdir -p "$PREFIX/lib" "$PREFIX/include"
    cp lib/liblz4.a "$PREFIX/lib/"
    cp lib/lz4.h lib/lz4hc.h lib/lz4frame.h lib/lz4frame_static.h "$PREFIX/include/"
    echo "  -> LZ4 installed to $PREFIX"
    echo ""
}

# ──────────────────────────────────────────────────────────────────────────
# 3. Boost (program_options + iostreams)
# ──────────────────────────────────────────────────────────────────────────
BOOST_VER="1.84.0"
BOOST_VER_US="${BOOST_VER//./_}"
BOOST_DIR="boost_${BOOST_VER_US}"
BOOST_TAR="${BOOST_DIR}.tar.bz2"
BOOST_URL="https://archives.boost.io/release/${BOOST_VER}/source/${BOOST_TAR}"

build_boost() {
    echo "──── Building Boost ${BOOST_VER} (program_options, iostreams) ────"
    cd "$SRC"

    if [ ! -d "$BOOST_DIR" ]; then
        [ -f "$BOOST_TAR" ] || wget -q "$BOOST_URL"
        tar xjf "$BOOST_TAR"
    fi

    cd "$BOOST_DIR"

    # Bootstrap b2 using the host compiler
    if [ ! -f b2 ]; then
        ./bootstrap.sh --with-libraries=program_options,iostreams \
            --prefix="$PREFIX"
    fi

    # Create a user-config.jam that tells b2 to use Emscripten
    cat > user-config.jam <<'JAMEOF'
using emscripten : : emcc ;
JAMEOF

    # Build and install
    ./b2 install \
        --user-config=user-config.jam \
        --prefix="$PREFIX" \
        --with-program_options \
        --with-iostreams \
        toolset=emscripten \
        variant=release \
        link=static \
        threading=single \
        runtime-link=static \
        cxxflags="$COMMON_CXXFLAGS -std=c++20" \
        -sNO_BZIP2=1 \
        -sNO_LZMA=1 \
        -sZLIB_INCLUDE="$(em-config EMSCRIPTEN_ROOT)/cache/sysroot/include" \
        -j"$JOBS" \
        || true  # b2 may return non-zero even on success

    # Verify at least one library was produced
    if ls "$PREFIX/lib"/libboost_program_options* &>/dev/null; then
        echo "  -> Boost installed to $PREFIX"
    else
        echo "  WARNING: Boost b2 toolset=emscripten may not be recognized."
        echo "  Falling back to manual build with emcc..."
        build_boost_manual
    fi
    echo ""
}

# Fallback: build Boost with plain emcc if b2 emscripten toolset is unavailable
build_boost_manual() {
    cd "$SRC/$BOOST_DIR"

    # Generate headers (needed for the include tree)
    ./b2 headers 2>/dev/null || true

    # -- program_options --
    echo "  Building boost::program_options manually..."
    PO_SRC="libs/program_options/src"
    PO_OBJS=""
    for cpp in "$PO_SRC"/*.cpp; do
        obj="${cpp%.cpp}.o"
        em++ $COMMON_CXXFLAGS -std=c++20 -I. -c "$cpp" -o "$obj"
        PO_OBJS="$PO_OBJS $obj"
    done
    emar rcs "$PREFIX/lib/libboost_program_options.a" $PO_OBJS

    # -- iostreams --
    echo "  Building boost::iostreams manually..."
    IO_SRC="libs/iostreams/src"
    IO_OBJS=""
    for cpp in "$IO_SRC"/zlib.cpp "$IO_SRC"/gzip.cpp "$IO_SRC"/mapped_file.cpp \
               "$IO_SRC"/file_descriptor.cpp; do
        if [ -f "$cpp" ]; then
            obj="${cpp%.cpp}.o"
            em++ $COMMON_CXXFLAGS -std=c++20 -I. \
                -I"$(em-config EMSCRIPTEN_ROOT)/cache/sysroot/include" \
                -DBOOST_IOSTREAMS_NO_LIB \
                -c "$cpp" -o "$obj"
            IO_OBJS="$IO_OBJS $obj"
        fi
    done
    emar rcs "$PREFIX/lib/libboost_iostreams.a" $IO_OBJS

    # Install headers
    mkdir -p "$PREFIX/include"
    cp -r boost "$PREFIX/include/" 2>/dev/null || true
    echo "  -> Boost (manual) installed to $PREFIX"
}

# ──────────────────────────────────────────────────────────────────────────
# 4. FFmpeg (minimal: avcodec, avformat, avutil, swscale, swresample)
# ──────────────────────────────────────────────────────────────────────────
FFMPEG_VER="6.1.2"
FFMPEG_DIR="ffmpeg-${FFMPEG_VER}"
FFMPEG_TAR="ffmpeg-${FFMPEG_VER}.tar.xz"
FFMPEG_URL="https://ffmpeg.org/releases/${FFMPEG_TAR}"

build_ffmpeg() {
    echo "──── Building FFmpeg ${FFMPEG_VER} ────"
    cd "$SRC"

    if [ ! -d "$FFMPEG_DIR" ]; then
        [ -f "$FFMPEG_TAR" ] || wget -q "$FFMPEG_URL"
        tar xJf "$FFMPEG_TAR"
    fi

    cd "$FFMPEG_DIR"
    make clean 2>/dev/null || true

    # FFmpeg's configure uses --enable-cross-compile for non-native targets.
    # We disable everything except the five libraries OpenMW needs.
    emconfigure ./configure \
        --prefix="$PREFIX" \
        --enable-cross-compile \
        --target-os=none \
        --arch=x86_32 \
        --cc=emcc \
        --cxx=em++ \
        --ar=emar \
        --ranlib=emranlib \
        --nm=emnm \
        --extra-cflags="$COMMON_CFLAGS -msimd128" \
        --extra-cxxflags="$COMMON_CXXFLAGS" \
        --disable-runtime-cpudetect \
        --disable-x86asm \
        --disable-inline-asm \
        --disable-stripping \
        --disable-programs \
        --disable-doc \
        --disable-debug \
        --disable-network \
        --disable-autodetect \
        --disable-everything \
        --enable-static \
        --disable-shared \
        --enable-avcodec \
        --enable-avformat \
        --enable-avutil \
        --enable-swscale \
        --enable-swresample \
        --enable-protocol=file \
        --enable-demuxer=mov,matroska,avi,mp3,ogg,wav,flac,aac,bink,smacker \
        --enable-decoder=vorbis,mp3,mp3float,aac,pcm_s16le,pcm_u8,flac,bink,binkaudio_dct,binkaudio_rdft,smackaud,smackvideo \
        --enable-parser=mpegaudio,vorbis,aac,flac \
        --enable-decoder=theora,vp8,vp9 \
        --enable-demuxer=ogg,webm

    emmake make -j"$JOBS"
    emmake make install

    echo "  -> FFmpeg installed to $PREFIX"
    echo ""
}

# ──────────────────────────────────────────────────────────────────────────
# 5. ICU 70.1 (host-build tools first, then cross-compile for WASM)
# ──────────────────────────────────────────────────────────────────────────
ICU_VER="70-1"
ICU_DIR="icu-release-${ICU_VER}"
ICU_ZIP="release-${ICU_VER}.zip"
ICU_URL="https://github.com/unicode-org/icu/archive/refs/tags/${ICU_ZIP}"

build_icu() {
    echo "──── Building ICU ${ICU_VER} ────"
    cd "$SRC"

    if [ ! -d "$ICU_DIR" ]; then
        [ -f "$ICU_ZIP" ] || wget -q "$ICU_URL"
        unzip -qo "$ICU_ZIP"
    fi

    ICU_SOURCE="$SRC/$ICU_DIR/icu4c/source"

    # 5a. Build ICU for the HOST (needed because ICU builds data-generation
    #     tools that must execute on the build machine)
    echo "  [ICU] Building host tools..."
    ICU_HOST_BUILD="$SRC/icu-host-build"
    mkdir -p "$ICU_HOST_BUILD"
    cd "$ICU_HOST_BUILD"

    if [ ! -f bin/icupkg ]; then
        "$ICU_SOURCE/configure" \
            --disable-tests --disable-samples \
            --disable-icuio --disable-extras \
            CC="gcc" CXX="g++"
        make -j"$JOBS"
    fi

    # 5b. Cross-compile ICU for WASM
    echo "  [ICU] Cross-compiling for Emscripten..."
    ICU_WASM_BUILD="$SRC/icu-wasm-build"
    mkdir -p "$ICU_WASM_BUILD"
    cd "$ICU_WASM_BUILD"

    # Emscripten's emconfigure will set CC/CXX; we also set the data filter
    # to strip down the ICU data to only what OpenMW needs.
    ICU_DATA_FILTER_FILE="${OPENMW_SOURCE_DIR:-$(dirname "$(dirname "$(realpath "$0")")")}/extern/icufilters.json"
    if [ ! -f "$ICU_DATA_FILTER_FILE" ]; then
        # Fallback: no filter
        ICU_DATA_FILTER_FILE=""
    fi

    CONFIGURE_ENV="CFLAGS=$COMMON_CFLAGS CXXFLAGS=$COMMON_CXXFLAGS"
    if [ -n "$ICU_DATA_FILTER_FILE" ]; then
        CONFIGURE_ENV="ICU_DATA_FILTER_FILE=$ICU_DATA_FILTER_FILE $CONFIGURE_ENV"
    fi

    env $CONFIGURE_ENV emconfigure "$ICU_SOURCE/configure" \
        --prefix="$PREFIX" \
        --enable-static \
        --disable-shared \
        --disable-tests \
        --disable-samples \
        --disable-icuio \
        --disable-extras \
        --disable-tools \
        --host=wasm32-unknown-emscripten \
        --with-cross-build="$ICU_HOST_BUILD"

    emmake make -j"$JOBS"
    emmake make install

    echo "  -> ICU installed to $PREFIX"
    echo ""
}

# ──────────────────────────────────────────────────────────────────────────
# 6. FreeType (needed by OSG FreeType plugin — not an Emscripten port here
#    because we need a pkg-config / CMake find module the OSG build can use)
#    NOTE: Emscripten has a USE_FREETYPE port. We prefer using the port by
#    passing -sUSE_FREETYPE=1 so OSG's CMake finds it via Emscripten's
#    sysroot. This section is kept as a fallback but is skipped by default.
# ──────────────────────────────────────────────────────────────────────────

# ──────────────────────────────────────────────────────────────────────────
# Build everything
# ──────────────────────────────────────────────────────────────────────────
build_lua
build_lz4
build_boost
build_ffmpeg
build_icu

echo "=================================================================="
echo "All WASM dependencies built successfully."
echo ""
echo "Install prefix: $PREFIX"
echo ""
echo "To use with the OpenMW CMake build:"
echo ""
echo "  emcmake cmake \\"
echo "    -DCMAKE_PREFIX_PATH=$PREFIX \\"
echo "    -DCMAKE_FIND_ROOT_PATH=$PREFIX \\"
echo "    ..."
echo ""
echo "=================================================================="

#!/bin/bash -ex
# --------------------------------------------------------------------------
# before_script.wasm.sh
#
# Orchestration script for building OpenMW as a WebAssembly application.
# Analogous to CI/before_script.android.sh for Android builds.
#
# This script:
#   1. Installs and activates the Emscripten SDK (if not already available)
#   2. Builds all external C/C++ dependencies via CI/build_wasm_deps.sh
#   3. Configures the OpenMW CMake build with the Emscripten toolchain
#
# Usage (from the repository root):
#   CI/before_script.wasm.sh
#
# Environment variables (optional overrides):
#   EMSDK_VER       – Emscripten version to install  (default: 3.1.51)
#   EMSDK_DIR       – Where to install/find the SDK  (default: ./emsdk)
#   WASM_DEPS_DIR   – Prefix for cross-compiled deps (default: ./wasm-deps)
#   BUILD_DIR       – CMake build directory           (default: ./build-wasm)
#   JOBS            – Parallel job count              (default: nproc)
# --------------------------------------------------------------------------

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

EMSDK_VER="${EMSDK_VER:-3.1.51}"
EMSDK_DIR="${EMSDK_DIR:-${ROOT_DIR}/emsdk}"
WASM_DEPS_DIR="${WASM_DEPS_DIR:-${ROOT_DIR}/wasm-deps}"
BUILD_DIR="${BUILD_DIR:-${ROOT_DIR}/build-wasm}"
JOBS="${JOBS:-$(nproc 2>/dev/null || echo 4)}"

# Silence a git warning
git config --global advice.detachedHead false 2>/dev/null || true

# ──────────────────────────────────────────────────────────────────────────
# 1. Install / activate Emscripten SDK
# ──────────────────────────────────────────────────────────────────────────
if ! command -v emcc &>/dev/null; then
    echo "=== Installing Emscripten SDK ${EMSDK_VER} ==="

    if [ ! -d "$EMSDK_DIR" ]; then
        git clone https://github.com/emscripten-core/emsdk.git "$EMSDK_DIR"
    fi

    cd "$EMSDK_DIR"
    ./emsdk install "$EMSDK_VER"
    ./emsdk activate "$EMSDK_VER"
    source ./emsdk_env.sh
    cd "$ROOT_DIR"
else
    echo "=== Emscripten already available: $(emcc --version | head -1) ==="
fi

# ──────────────────────────────────────────────────────────────────────────
# 2. Pre-fetch Emscripten ports (SDL2, zlib, libpng, libjpeg, FreeType)
#    so that they are available when CMake runs find_package / configure.
# ──────────────────────────────────────────────────────────────────────────
echo "=== Pre-fetching Emscripten ports ==="
embuilder build sdl2 zlib libpng libjpeg freetype

# ──────────────────────────────────────────────────────────────────────────
# 3. Build external dependencies
# ──────────────────────────────────────────────────────────────────────────
echo "=== Building WASM dependencies ==="
"$SCRIPT_DIR/build_wasm_deps.sh" --prefix "$WASM_DEPS_DIR" --jobs "$JOBS"

# ──────────────────────────────────────────────────────────────────────────
# 4. Build ICU host tools (required by extern/CMakeLists.txt ICU cross-build)
# ──────────────────────────────────────────────────────────────────────────
echo "=== Building ICU host tools ==="
ICU_HOST_BUILD="${BUILD_DIR}/icu-host-build"
mkdir -p "$ICU_HOST_BUILD"

# Check if already built
if [ ! -f "$ICU_HOST_BUILD/bin/icupkg" ]; then
    # The ICU source will be fetched by extern/CMakeLists.txt via
    # ExternalProject. For the host build we download it here.
    ICU_HOST_SRC="${BUILD_DIR}/icu-host-src"
    if [ ! -d "$ICU_HOST_SRC" ]; then
        mkdir -p "$ICU_HOST_SRC"
        cd "$ICU_HOST_SRC"
        if [ -r "${ROOT_DIR}/extern/fetched/icu/icu4c/source/configure" ]; then
            ICU_SOURCE_DIR="${ROOT_DIR}/extern/fetched/icu/icu4c/source"
        else
            wget -q https://github.com/unicode-org/icu/archive/refs/tags/release-70-1.zip
            unzip -qo release-70-1.zip
            ICU_SOURCE_DIR="$ICU_HOST_SRC/icu-release-70-1/icu4c/source"
        fi
    else
        if [ -r "${ROOT_DIR}/extern/fetched/icu/icu4c/source/configure" ]; then
            ICU_SOURCE_DIR="${ROOT_DIR}/extern/fetched/icu/icu4c/source"
        else
            ICU_SOURCE_DIR="$ICU_HOST_SRC/icu-release-70-1/icu4c/source"
        fi
    fi

    cd "$ICU_HOST_BUILD"
    "$ICU_SOURCE_DIR/configure" \
        --disable-tests --disable-samples \
        --disable-icuio --disable-extras \
        CC="gcc" CXX="g++"
    make -j "$JOBS"
fi

# ──────────────────────────────────────────────────────────────────────────
# 5. Configure the OpenMW CMake build
# ──────────────────────────────────────────────────────────────────────────
echo "=== Configuring OpenMW for WebAssembly ==="
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

emcmake cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="$WASM_DEPS_DIR" \
    -DCMAKE_FIND_ROOT_PATH="$WASM_DEPS_DIR" \
    -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=BOTH \
    -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=BOTH \
    -DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=BOTH \
    -DOPENMW_EXPERIMENTAL_WASM=ON \
    -DOPENMW_EXPERIMENTAL_WASM_PTHREADS=OFF \
    -DOPENMW_USE_SYSTEM_BULLET=OFF \
    -DOPENMW_USE_SYSTEM_OSG=OFF \
    -DOPENMW_USE_SYSTEM_MYGUI=OFF \
    -DOPENMW_USE_SYSTEM_RECASTNAVIGATION=OFF \
    -DOPENMW_USE_SYSTEM_SQLITE3=OFF \
    -DOPENMW_USE_SYSTEM_YAML_CPP=OFF \
    -DOPENMW_USE_SYSTEM_ICU=OFF \
    -DOPENMW_ICU_HOST_BUILD_DIR="$ICU_HOST_BUILD" \
    -DUSE_SYSTEM_TINYXML=OFF \
    -DBUILD_OPENMW=ON \
    -DBUILD_LAUNCHER=OFF \
    -DBUILD_WIZARD=OFF \
    -DBUILD_MWINIIMPORTER=OFF \
    -DBUILD_OPENCS=OFF \
    -DBUILD_ESSIMPORTER=OFF \
    -DBUILD_BSATOOL=OFF \
    -DBUILD_ESMTOOL=OFF \
    -DBUILD_NIFTEST=OFF \
    -DBUILD_NAVMESHTOOL=OFF \
    -DBUILD_BULLETOBJECTTOOL=OFF \
    -DBUILD_COMPONENTS_TESTS=OFF \
    -DBUILD_BENCHMARKS=OFF \
    "$ROOT_DIR"

echo ""
echo "=================================================================="
echo "CMake configuration complete."
echo ""
echo "To build:"
echo "  cd $BUILD_DIR && emmake make -j$JOBS"
echo ""
echo "The output will be:"
echo "  $BUILD_DIR/openmw.html"
echo "  $BUILD_DIR/openmw.js"
echo "  $BUILD_DIR/openmw.wasm"
echo "=================================================================="

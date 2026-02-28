# Building OpenMW for WebAssembly

This guide covers building OpenMW as a WebAssembly application that runs in modern web browsers.

## Prerequisites

- [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html) (3.1.50+)
- CMake 3.16+
- Standard build tools (make, ninja)

```bash
# Install and activate Emscripten
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install latest
./emsdk activate latest
source ./emsdk_env.sh
```

## Building Dependencies

OpenMW requires several C++ libraries that must be cross-compiled for WASM.

### Quick Start (Lua + LZ4)

```bash
./CI/build_emscripten_deps.sh ~/openmw-wasm-deps
```

### Manual Dependency Builds

#### Lua 5.4

```bash
wget https://www.lua.org/ftp/lua-5.4.7.tar.gz
tar xzf lua-5.4.7.tar.gz && cd lua-5.4.7/src
emcc -O2 -c lapi.c lcode.c lctype.c ldebug.c ldo.c ldump.c lfunc.c lgc.c \
    llex.c lmem.c lobject.c lopcodes.c lparser.c lstate.c lstring.c ltable.c \
    ltm.c lundump.c lvm.c lzio.c lauxlib.c lbaselib.c lcorolib.c ldblib.c \
    liolib.c lmathlib.c loadlib.c loslib.c lstrlib.c ltablib.c lutf8lib.c linit.c
emar rcs liblua.a *.o
```

#### LZ4

```bash
git clone --depth 1 --branch v1.10.0 https://github.com/lz4/lz4.git
cd lz4/build/cmake
emcmake cmake . -DCMAKE_INSTALL_PREFIX=$PREFIX \
    -DBUILD_SHARED_LIBS=OFF -DLZ4_BUILD_CLI=OFF
emmake make -j$(nproc) install
```

#### Bullet Physics

```bash
git clone --depth 1 https://github.com/bulletphysics/bullet3.git
cd bullet3
emcmake cmake -B build \
    -DCMAKE_INSTALL_PREFIX=$PREFIX \
    -DBUILD_BULLET2_DEMOS=OFF \
    -DBUILD_UNIT_TESTS=OFF \
    -DBUILD_EXTRAS=OFF \
    -DBUILD_CPU_DEMOS=OFF \
    -DBUILD_SHARED_LIBS=OFF
emmake make -C build -j$(nproc) install
```

#### OpenSceneGraph (OSG)

```bash
git clone --depth 1 https://github.com/openscenegraph/OpenSceneGraph.git
cd OpenSceneGraph
emcmake cmake -B build \
    -DCMAKE_INSTALL_PREFIX=$PREFIX \
    -DOSG_GL3_AVAILABLE=OFF \
    -DOSG_GLES2_AVAILABLE=OFF \
    -DOSG_GLES3_AVAILABLE=ON \
    -DOSG_GL_DISPLAYLISTS_AVAILABLE=OFF \
    -DOSG_GL_FIXED_FUNCTION_AVAILABLE=OFF \
    -DOSG_GL_VERTEX_FUNCS_AVAILABLE=OFF \
    -DDYNAMIC_OPENSCENEGRAPH=OFF \
    -DDYNAMIC_OPENTHREADS=OFF \
    -DBUILD_OSG_APPLICATIONS=OFF \
    -DBUILD_OSG_PLUGINS_BY_DEFAULT=OFF \
    -DBUILD_OSG_PLUGIN_BMP=ON \
    -DBUILD_OSG_PLUGIN_DDS=ON \
    -DBUILD_OSG_PLUGIN_JPEG=ON \
    -DBUILD_OSG_PLUGIN_OSG=ON \
    -DBUILD_OSG_PLUGIN_PNG=ON \
    -DBUILD_OSG_PLUGIN_TGA=ON \
    -DBUILD_OSG_PLUGIN_KTX=ON
emmake make -C build -j$(nproc) install
```

#### MyGUI

```bash
git clone --depth 1 https://github.com/MyGUI/mygui.git
cd mygui
emcmake cmake -B build \
    -DCMAKE_INSTALL_PREFIX=$PREFIX \
    -DMYGUI_RENDERSYSTEM=1 \
    -DMYGUI_BUILD_DEMOS=OFF \
    -DMYGUI_BUILD_TOOLS=OFF \
    -DMYGUI_BUILD_PLUGINS=OFF \
    -DMYGUI_BUILD_TESTS=OFF
emmake make -C build -j$(nproc) install
```

#### Boost

```bash
wget https://archives.boost.io/release/1.86.0/source/boost_1_86_0.tar.gz
tar xzf boost_1_86_0.tar.gz && cd boost_1_86_0
./bootstrap.sh
echo "using emscripten : : em++ ;" > user-config.jam
./b2 --user-config=user-config.jam toolset=emscripten \
    --with-program_options --with-iostreams --with-filesystem --with-system \
    link=static variant=release install --prefix=$PREFIX
```

## Building OpenMW

```bash
cd openmw
emcmake cmake -B build \
    -C cmake/emscripten-wasm.cmake \
    -DCMAKE_PREFIX_PATH=$PREFIX \
    -DCMAKE_FIND_ROOT_PATH=$PREFIX \
    -DCMAKE_BUILD_TYPE=Release
emmake make -C build -j$(nproc)
```

The output will be:
- `build/openmw.html` - Main HTML page
- `build/openmw.js` - JavaScript glue code
- `build/openmw.wasm` - WebAssembly binary
- `build/openmw.data` - Preloaded data (if any)

## Hosting

The WASM build requires specific HTTP headers for SharedArrayBuffer support:

```
Cross-Origin-Opener-Policy: same-origin
Cross-Origin-Embedder-Policy: require-corp
```

Example with Python:

```python
import http.server
class Handler(http.server.SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header("Cross-Origin-Opener-Policy", "same-origin")
        self.send_header("Cross-Origin-Embedder-Policy", "require-corp")
        super().end_headers()
http.server.HTTPServer(("", 8000), Handler).serve_forever()
```

## Browser Requirements

- Chrome 94+ / Edge 94+ (recommended)
- Firefox 100+ (with SharedArrayBuffer enabled)
- WebGL 2.0 support required
- File System Access API required for data loading (Chrome/Edge only)
- Alternatively, drag-and-drop file upload works in all browsers

## Usage

1. Open the hosted `openmw.html` in a browser
2. Click "Select Morrowind Data Folder" and choose your `Data Files` directory
3. The game will load and begin
4. On subsequent visits, cached data will be restored automatically from OPFS

### Keyboard Shortcuts
- **\`** (tilde) - Toggle developer console overlay
- **F11** or call `__openmwToggleFullscreen()` - Toggle browser fullscreen

### JavaScript API
- `__openmwPickDataDirectory()` - Open file picker for game data
- `__openmwToggleFullscreen()` - Toggle fullscreen mode
- `__openmwCacheToOPFS()` - Cache game data to OPFS for persistence
- `__openmwRestoreFromOPFS()` - Restore game data from OPFS cache
- `__openmwIsDataReady()` - Check if game data is loaded
- `__openmwGetMemoryUsage()` - Get WASM heap memory usage

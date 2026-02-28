# WebAssembly (WASM) Porting Roadmap for OpenMW

This document outlines the steps and considerations required to port the OpenMW engine to WebAssembly (WASM) so that it is fully playable in modern web browsers like Google Chrome.

## 1. Toolchain and Build System
The primary compiler for WebAssembly from C++ is [Emscripten](https://emscripten.org/).
- **CMake Integration:** Modify `CMakeLists.txt` files to support Emscripten (`emcc`, `em++`).
- **Target Architecture:** Add a new target architecture for WASM alongside existing platforms (Windows, Linux, macOS, Android).
- **Emscripten Flags:** Configure compilation and linking flags (`-s USE_SDL=2`, `-s USE_WEBGL2=1`, `-s FULL_ES3=1`, `-s ALLOW_MEMORY_GROWTH=1`, `-s USE_PTHREADS=1`, etc.).

## 2. Dependency Management
OpenMW has a significant list of dependencies that must also be compiled to WebAssembly or replaced with web-native alternatives.
- **SDL2:** Supported natively by Emscripten. Needs the `-s USE_SDL=2` flag.
- **OpenSceneGraph (OSG):** Needs to be compiled for GLES2/GLES3 (WebGL/WebGL2). Support for WebGL in OSG exists but may require patching or configuration specific to Emscripten.
- **Bullet (Physics):** Can be compiled to WASM. Alternatives like ammo.js exist, but compiling the existing Bullet C++ is preferred.
- **Boost:** Most Boost header-only libraries will work. Compiled libraries (like filesystem) might need Emscripten configurations or replacements with `std::filesystem` where possible.
- **OpenAL:** Emscripten has an OpenAL wrapper.
- **FFmpeg:** Needs to be compiled to WASM. Might be challenging; consider alternative decoders for Web Audio if video playback is too heavy.
- **Lua/LuaJIT:** Standard Lua compiles easily to WASM. LuaJIT is assembly-heavy and will not work; fallback to standard Lua is required for the web build.
- **MyGUI:** Needs to be compiled to WASM.
- **Qt:** Not required for the game engine itself, only for OpenMW-CS and launcher. Can be excluded for the web client.

## 3. Main Loop Refactoring
Web browsers do not allow blocking infinite loops like the traditional desktop application `while (!done)` loop.
- **Current Architecture:** In `apps/openmw/engine.cpp`, the main rendering loop blocks the main thread.
- **Emscripten Approach:** The main loop must be handed over to the browser using `emscripten_set_main_loop()` or `emscripten_set_main_loop_arg()`.
- **Refactoring:** Extract the body of the `while (!mViewer->done() && !mStateManager->hasQuitRequest())` loop in `OMW::Engine::go()` into a separate function that can be called repeatedly by the browser's requestAnimationFrame.

## 4. Graphics and Rendering
Web browsers use WebGL (based on OpenGL ES).
- **WebGL 2.0:** Target WebGL 2.0 (OpenGL ES 3.0) as it supports more modern features required by OpenMW.
- **Shaders:** Ensure all GLSL shaders are compatible with GLSL ES 3.00.
- **Extensions:** Some desktop OpenGL extensions used by OSG/OpenMW might not be available in WebGL. Fallbacks or polyfills will be needed.

## 5. File System and Assets (The Data Path)
Morrowind relies heavily on gigabytes of data files (.esm, .esp, .bsa, textures, meshes).
- **Virtual File System (VFS):** Browsers cannot directly read the local filesystem for security reasons.
- **Preloading:** Small assets can be preloaded using Emscripten's `--preload-file`.
- **IndexedDB / File System API:** For large Morrowind data files, users will need to "upload" or select their local Morrowind installation folder via the browser's File System Access API. The data can then be cached in the browser using IndexedDB (via Emscripten's IDBFS).
- **Asynchronous Loading:** Modify OpenMW's VFS to handle asynchronous reads if loading chunks directly from the user's local disk via web APIs.

## 6. Multithreading
OpenMW uses background threads for physics, resource loading, and paging.
- **Web Workers:** Emscripten supports pthreads via Web Workers and `SharedArrayBuffer`.
- **Requirements:** Requires the server hosting the WASM file to send specific headers (`Cross-Origin-Opener-Policy: same-origin` and `Cross-Origin-Embedder-Policy: require-corp`) for `SharedArrayBuffer` to be enabled in modern browsers.

## 7. Audio
- Use Emscripten's OpenAL implementation which maps directly to the Web Audio API.

## 8. Input and Controls
- **Mouse & Keyboard:** SDL2 through Emscripten handles standard keyboard and mouse events well.
- **Pointer Lock:** The game will need to use the Pointer Lock API (handled by SDL2) to capture the mouse for 3D camera movement. Browsers require a user interaction (like a click) before locking the pointer.

## Execution Plan Summary
1. **Setup CI/Build:** Create an Emscripten toolchain file and fix CMake errors for dependencies.
2. **Compile Dependencies:** Build OSG, Bullet, MyGUI, and standard Lua for WASM.
3. **Refactor Main Loop:** Replace the blocking loop in `engine.cpp` with `emscripten_set_main_loop`.
4. **VFS Integration:** Implement IDBFS and File System Access API for user data.
5. **Graphics Fixes:** Resolve WebGL shader and OSG compatibility issues.
6. **Testing & Optimization:** Profile and test the build in Google Chrome.

## Current Bootstrap Status
- Added an experimental CMake switch: `-DOPENMW_EXPERIMENTAL_WASM=ON`.
- The switch auto-enables when CMake is configured with an Emscripten toolchain and currently:
  - Disables desktop-only applications and tools (launcher, OpenMW-CS, importers, inspectors, etc.).
  - Forces standard Lua (`USE_LUAJIT=OFF`) to avoid LuaJIT incompatibilities on WASM.
  - Applies baseline Emscripten linker flags for SDL2, WebGL 2.0, ES3, memory growth, and forced virtual filesystem support.
  - Enables pthread/Web Worker support by default, with an opt-out switch (`-DOPENMW_EXPERIMENTAL_WASM_PTHREADS=OFF`) for early single-threaded bring-up or hosts without cross-origin isolation headers.
  - Exports C functions for browser JavaScript interop (`openmw_wasm_notify_data_ready`, etc.) and bundles Emscripten runtime methods (`UTF8ToString`, `ccall`, `cwrap`).
  - Sets initial memory to 256 MB and stack size to 5 MB.
  - Uses a custom HTML shell template (`files/wasm/openmw_shell.html`) with loading progress UI and data folder picker.
- Refactors the desktop game loop body into `Engine::runMainLoopIteration(...)` so it can be reused by a browser-driven frame callback.
- Adds an Emscripten-specific `emscripten_set_main_loop_arg(...)` integration that executes one `runMainLoopIteration(...)` per browser frame and cancels cleanly when the engine requests quit.
- Consolidates post-loop shutdown/persistence in `Engine::shutdownAfterMainLoop()` so desktop and WASM paths share the same cleanup behavior.
- Bootstraps an IDBFS mount at `/persistent` for Emscripten builds, redirects HOME/XDG paths into that mount, and triggers startup/shutdown syncs so browser sessions can persist configs/saves across reloads.
- Adds a pthread-hosting diagnostic in Emscripten builds: when compiled with pthread support, startup now warns in the browser console if the page is not cross-origin isolated (missing COOP/COEP headers).
- Hardens Emscripten persistence bootstrap/shutdown scripts with FS/IDBFS availability checks and pre-creates XDG config/data directories under `/persistent/home` before path redirection.
- Registers periodic/runtime persistence sync triggers (`visibilitychange`, `pagehide`, `beforeunload`, and interval-based sync) while coalescing overlapping sync requests to avoid IDBFS race conditions; the interval is configurable via `OPENMW_WASM_PERSISTENT_SYNC_INTERVAL_MS` and the mount root can be overridden with `OPENMW_WASM_PERSISTENT_ROOT`.

## Platform & Component Porting Status
- **Crash catcher**: Excluded from Emscripten builds (POSIX signals, `fork()`, `ptrace()` unavailable in WASM). The header-level guard makes `crashCatcherInstall` a no-op, and CMake skips compiling `crashcatcher.cpp`.
- **Emscripten path resolver**: Added `EmscriptenPath` class (`components/files/emscriptenpath.hpp/cpp`) providing XDG-compatible paths under the Emscripten virtual filesystem (`/persistent/home/.config`, `/persistent/home/.local/share`, `/tmp`, etc.). Integrated into `fixedpath.hpp` as the platform target for `__EMSCRIPTEN__`.
- **Thread priority**: `setCurrentThreadIdlePriority()` returns a no-op under Emscripten (`pthread_setschedparam`/`SCHED_IDLE` unavailable). Falls back to informational log message.
- **SDL video wrapper**: Gamma ramp functions (`SDL_GetWindowGammaRamp`, `SDL_SetWindowGammaRamp`) guarded with `#ifndef __EMSCRIPTEN__` as they are unsupported in the browser SDL2 port.
- **SDL graphics window**: WebGL 2.0 (OpenGL ES 3.0) context requested via `SDL_GL_SetAttribute` under `__EMSCRIPTEN__`, placed before Android/gl4es handling.
- **Engine window creation**: Emscripten-specific path skips fullscreen flags, MSAA, window borders, high-DPI hints, minimize-on-focus-loss, debug GL contexts, and stereo rendering. Uses `SDL_WINDOWPOS_UNDEFINED` and `SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE`.
- **Engine data loading**: `std::async` replaced with synchronous `mWorld->loadData(...)` under Emscripten to avoid Web Worker requirements for data loading.
- **WASM performance defaults**: At startup, shadows are disabled, viewing distance capped to 4096, MSAA set to 0, and reverse-z depth buffer disabled.
- **Shadow technique**: `GL_DEPTH_CLAMP` and `osg::ClipControl` guarded with `#ifndef __EMSCRIPTEN__` (unavailable in WebGL 2.0).
- **Compute shaders**: Disabled under Emscripten in `ripples.cpp` (WebGL 2.0 lacks compute shader support).
- **Menu video thread**: Background video playback thread guarded with `__EMSCRIPTEN_PTHREADS__` check; skipped when pthreads are unavailable.
- **Debug/logging**: Emscripten-specific handling for colored output detection (always false in browser), `isatty` checks bypassed, SDL error dialog shown unconditionally.
- **Browser data bridge**: `wasmfilepicker.hpp/cpp` provides:
  - JavaScript `__openmwPickDataDirectory()` function for File System Access API directory selection.
  - Recursive directory upload into Emscripten virtual FS at `/gamedata`.
  - C++ API (`isDataReady()`, `getDataPath()`, `listUploadedFiles()`) for engine integration.
  - Exported C functions (`openmw_wasm_notify_data_ready`, `openmw_wasm_is_data_ready`, `openmw_wasm_get_data_path`).
- **HTML shell template**: Custom `openmw_shell.html` with loading progress bar, "Select Morrowind Data Folder" button, console output overlay (toggle with tilde key), and responsive canvas layout.

## Dependency Cross-Compilation Status

Build infrastructure for cross-compiling all major OpenMW dependencies to WebAssembly is now in place. The two main scripts are:

- **`CI/build_wasm_deps.sh`**: Builds the external dependencies that are not managed by FetchContent — Lua 5.4, LZ4, Boost (program_options + iostreams), FFmpeg (avcodec/avformat/avutil/swscale/swresample), and ICU 70.1 (with host-build for tools).
- **`CI/before_script.wasm.sh`**: Full orchestration script that installs Emscripten SDK, pre-fetches Emscripten ports, runs `build_wasm_deps.sh`, builds ICU host tools, and configures the OpenMW CMake build.

### Dependencies provided by Emscripten ports (no build needed)
| Dependency | Port flag | Notes |
|---|---|---|
| SDL2 | `-sUSE_SDL=2` | Window, input, audio device |
| zlib | `-sUSE_ZLIB=1` | Compression |
| libpng | `-sUSE_LIBPNG=1` | PNG image loading (OSG plugin) |
| libjpeg | `-sUSE_LIBJPEG=1` | JPEG image loading (OSG plugin) |
| FreeType | `-sUSE_FREETYPE=1` | Font rendering (OSG plugin) |
| OpenAL | `-lopenal` | 3D audio via Web Audio API |

### Dependencies built by `CI/build_wasm_deps.sh`
| Dependency | Version | Notes |
|---|---|---|
| Lua | 5.4.7 | Standard Lua (LuaJIT cannot target WASM) |
| LZ4 | 1.9.4 | Static library via `emcc` |
| Boost | 1.84.0 | `program_options` + `iostreams`; b2 emscripten toolset with manual fallback |
| FFmpeg | 6.1.2 | Minimal build: only codecs/formats OpenMW uses (Bink, Vorbis, MP3, etc.) |
| ICU | 70.1 | Host-build for tools, then cross-compile static libs for WASM |

### Dependencies built by FetchContent (in `extern/CMakeLists.txt`)
| Dependency | Version | Emscripten adaptations |
|---|---|---|
| Bullet Physics | 3.17 | Double precision ON; multithreading OFF for Emscripten |
| OpenSceneGraph | 3.6 (OpenMW fork) | GLES3 profile forced (WebGL 2.0); GL1/GL2 disabled; COLLADA plugin disabled |
| MyGUI | 3.4.3 | OSG render system 4; static build |
| RecastNavigation | (OpenMW fork) | Static build, no changes needed |
| SQLite3 | 3.41.1 | Amalgamation, no changes needed |
| yaml-cpp | 0.8.0+ | Static build, no changes needed |

### CMake integration
- `CMakeLists.txt` now forces all `OPENMW_USE_SYSTEM_*` options to `OFF` when `OPENMW_IS_EMSCRIPTEN` is true, ensuring FetchContent-managed dependencies are used.
- Emscripten port compile/link flags (`-sUSE_SDL=2`, `-sUSE_ZLIB=1`, etc.) are added globally so that both OpenMW and its FetchContent sub-builds pick them up.
- Dummy CMake imported targets are created for SDL2, OpenAL, ZLIB, and OpenGL so that existing `find_package()` / `target_link_libraries()` calls continue to work.
- The OSG FetchContent block sets `OPENGL_PROFILE=GLES3` and disables all desktop GL profiles.
- ICU ExternalProject now supports Emscripten cross-compilation alongside Android.
- Boost `find_package` falls back from CONFIG to MODULE mode for cross-compiled builds.

## CMake Build Blocker Fixes

Concrete issues that would prevent the WASM build from configuring and linking have been addressed:

- **OpenGL linking**: `${OPENGL_gl_LIBRARY}` replaced with `OpenGL::GL` imported target in `components/CMakeLists.txt`, compatible with the Emscripten dummy target.
- **OpenAL linking**: `${OPENAL_LIBRARY}` (empty for Emscripten) replaced with `-lopenal` link option that maps to Emscripten's Web Audio wrapper.
- **Platform-specific library guards**: `find_package(Threads)`, `${CMAKE_THREAD_LIBS_INIT}` linking, and `CheckLuaCustomAllocator.cmake` (uses `try_run()`, fails during cross-compile) guarded with `NOT OPENMW_IS_EMSCRIPTEN`.
- **WholeArchive.cmake**: Added `CMAKE_CXX_COMPILER_ID "Emscripten"` to the Clang/GNU branch (wasm-ld supports `--whole-archive`).
- **Executable target**: Emscripten path in `apps/openmw/CMakeLists.txt` produces `.html`+`.js`+`.wasm` outputs via `add_executable` with `.html` suffix.
- **WASM openmw.cfg**: Bootstrap config (`files/wasm/openmw.cfg`) pointing data paths to Emscripten virtual filesystem locations (`/gamedata`, `/persistent/home`).

## GLSL ES 3.00 Shader Compatibility

An automatic GLSL 120 → GLSL ES 300 source-level translator has been added to `ShaderManager` (`components/shader/shadermanager.cpp`), activated at compile time for Emscripten builds (`#ifdef __EMSCRIPTEN__`). This avoids duplicating the 35+ compatibility shader files.

### Conversions performed by `convertToGLSLES300()`:

| GLSL 1.20 (Desktop) | GLSL ES 3.00 (WebGL 2.0) | Notes |
|---|---|---|
| `#version 120` | `#version 300 es` + precision qualifiers | `highp float/int/sampler` |
| `attribute` | `in` | Vertex shader only |
| `varying` | `out` / `in` | Vertex → `out`, Fragment → `in` |
| `centroid varying` | `centroid out` / `centroid in` | Same rule |
| `texture2D()` | `texture()` | Also `texture3D`, `textureCube`, `shadow2D` |
| `gl_Vertex` | `osg_Vertex` | + `in vec4` declaration |
| `gl_Normal` | `osg_Normal` | + `in vec3` declaration |
| `gl_Color` | `osg_Color` | + `in vec4` declaration |
| `gl_MultiTexCoordN` | `osg_MultiTexCoordN` | + `in vec4` declarations |
| `gl_ModelViewMatrix` | `osg_ModelViewMatrix` | + `uniform mat4` declaration |
| `gl_ModelViewProjectionMatrix` | `osg_ModelViewProjectionMatrix` | + `uniform mat4` declaration |
| `gl_ProjectionMatrix` | `osg_ProjectionMatrix` | + `uniform mat4` declaration |
| `gl_NormalMatrix` | `osg_NormalMatrix` | + `uniform mat3` declaration |
| `gl_TextureMatrix[n]` | `osg_TextureMatrix[n]` | + `uniform mat4[8]` declaration |
| `gl_FrontMaterial` | `osg_FrontMaterial` | + struct + `uniform` declaration |
| `gl_LightModel` | `osg_LightModel` | + struct + `uniform` declaration |
| `gl_FragData[0]` / `gl_FragData[1]` | `osg_FragColor` / `osg_FragData1` | + `layout(location=N) out vec4` |
| `gl_FragColor` | `osg_FragColor` | + `layout(location=0) out vec4` |
| `gl_ClipVertex = ...;` | Removed (comment) | Not available in ES 3.0 |
| `#extension GL_ARB_*` | Commented out | Features built-in to ES 3.0 |

### Additional lighting changes:

- **FFP lighting override**: `LightManager` (`components/sceneutil/lightmanager.cpp`) forces `PerObjectUniform` mode on Emscripten when the user selects FFP (`legacy`) mode, since `gl_LightSource[]` is not available in GLES 3.0.
- The `@getLight` define resolves to `LightBuffer` (custom uniform array) instead of `gl_LightSource` in non-FFP modes.

### How it works:

1. The converter runs in `ShaderManager::getShader()` after `createSourceFromTemplate()` completes, operating on the fully-resolved shader source (includes expanded, `@defines` substituted).
2. Linked shaders (e.g., `lib/core/vertex.glsl`) are also converted since they pass through `getShader()` independently.
3. Declarations for replaced built-in identifiers are only added when the identifier is actually used in the source, keeping the output minimal.
4. Word-boundary-aware identifier matching prevents false replacements (e.g., `gl_ModelViewMatrix` won't match inside `gl_ModelViewProjectionMatrix`).

## C++ Source-Level GLES3 / WebGL 2.0 Fixes

Desktop OpenGL APIs that don't exist in WebGL 2.0 / OpenGL ES 3.0 have been guarded with `#ifdef __EMSCRIPTEN__` / `#ifndef __EMSCRIPTEN__` compile-time conditionals:

| File | Issue | Fix |
|---|---|---|
| `apps/openmw/mwrender/water.cpp` | `GL_DEPTH_CLAMP` (desktop GL 3.2+) | Bypassed; draw without depth clamp on Emscripten |
| `components/myguiplatform/myguirendermanager.cpp` | `glEnableClientState`, `glVertexPointer`, `glColorPointer`, `glTexCoordPointer` (FFP client-state API) | Replaced with `glVertexAttribPointer` / `glEnableVertexAttribArray` using OSG standard attribute locations (0=position, 3=color, 8=texcoord) |
| `components/myguiplatform/myguirendermanager.cpp` | `GL_LIGHTING`, `GL_TEXTURE_2D` state modes | Guarded out (not valid state enums in GLES 3.0) |
| `components/sceneutil/lightmanager.cpp` | `DisableLight`, `FFPLightStateAttribute`, `StateSetGeneratorFFP` classes using `glLight*` / `GL_LIGHT0..7` | Entire FFP lighting classes wrapped in `#ifndef __EMSCRIPTEN__`; `initFFP()` guarded; `setLightingMethod()` FFP case emits error log |
| `components/stereo/multiview.cpp` | `glTextureView` (desktop GL 4.3+) | Guarded out with warning log; stereo/multiview not supported on WebGL2 |

## Remaining Work
- **Large asset streaming**: Current file picker loads all data into memory; consider chunked/lazy loading for large Morrowind installations.
- **Audio decoder**: Verify FFmpeg/audio decoding works under Emscripten or provide fallback.
- **Input handling**: Verify pointer lock, keyboard, and gamepad input through Emscripten SDL2.
- **Testing and profiling**: End-to-end testing in Chrome with actual Morrowind data, performance profiling and optimization.
- **End-to-end WASM build validation**: Run `CI/before_script.wasm.sh` on a CI runner with Emscripten to verify all dependencies compile and link.

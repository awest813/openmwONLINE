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

## GLSL ES 3.00 Shader Compatibility

The shader manager (`components/shader/shadermanager.cpp`) now includes an automatic GLSL ES 3.00 source transformation pass for Emscripten builds. When `__EMSCRIPTEN__` is defined, `transformShaderToGLES3()` runs after template preprocessing and before shader compilation. The transformation handles:

- **Version directives**: `#version 120` / `#version 330` → `#version 300 es` with `precision highp float/int/sampler2D/sampler2DShadow/samplerCube` qualifiers.
- **Variable qualifiers**: `varying` → `out` (vertex) / `in` (fragment); `centroid varying` → `centroid out` / `centroid in`; `attribute` → `in`.
- **Vertex attribute builtins**: `gl_Vertex` → `osg_Vertex`, `gl_Normal` → `osg_Normal`, `gl_Color` → `osg_Color`, `gl_MultiTexCoord0-7` → `osg_MultiTexCoord0-7`.
- **Matrix builtins**: `gl_ModelViewMatrix` → `osg_ModelViewMatrix`, `gl_ProjectionMatrix` → `osg_ProjectionMatrix`, `gl_ModelViewProjectionMatrix` → `osg_ModelViewProjectionMatrix`, `gl_NormalMatrix` → `osg_NormalMatrix`.
- **Texture matrices**: `gl_TextureMatrix[N]` → `omw_TextureMatrixN` (custom uniforms, N=0..7).
- **Material properties**: `gl_FrontMaterial.*` → `omw_FrontMaterial.*` (custom uniform struct).
- **Fog properties**: `gl_Fog.*` → `omw_Fog.*` (custom uniform struct).
- **Fragment outputs**: `gl_FragData[N]` → `omw_FragDataN` (declared as `layout(location=N) out vec4`).
- **Clip vertex**: `gl_ClipVertex` assignments removed (unavailable in WebGL 2.0).
- **Texture lookup functions**: `texture2D` → `texture`, `texture3D` → `texture`, `textureCube` → `texture`, `shadow2DProj` → `textureProj`.
- **Desktop-only extensions**: `GL_ARB_uniform_buffer_object` and `GL_EXT_gpu_shader4` extension directives removed.
- **OSG pragmas**: `#pragma import_defines(...)` removed.

GLES3 preambles are prepended to vertex and fragment shaders declaring OSG vertex attributes, matrix uniforms, and custom struct uniforms for material/fog/texture matrix state.

### GLES3 Fixed-Function Uniform Providers

A new utility (`components/sceneutil/gles3uniforms.hpp/cpp`) provides C++ functions to mirror fixed-function OpenGL state as uniforms for GLES3 builds:

- **`applyMaterial()`**: Extracts `osg::Material` properties (emission, ambient, diffuse, specular, shininess) and sets them as `omw_FrontMaterial.*` uniforms.
- **`applyFog()`**: Sets `omw_Fog.*` uniforms (color, start, end, scale) from fog parameters.
- **`applyTextureMatrix()`**: Sets `omw_TextureMatrixN` uniforms from `osg::TexMat` state.
- **`applyAllDefaults()`**: Applies default values for all above to a root state set.

Integration points:
- Root scene node: Default material/fog/texture matrix uniforms applied at initialization.
- NIF loader: Per-object material uniforms mirrored when `osg::Material` is set.
- Fog state updater: Fog uniforms updated each frame alongside `osg::Fog` state attribute.

## Config File Bootstrapping

WASM builds now auto-generate a minimal `openmw.cfg` at first run (when no config exists in the IDBFS-backed persistent storage). The generated config includes:
- `data=/gamedata` pointing to the browser file picker upload directory.
- `content=Morrowind.esm` for the base game.
- `fallback-archive=Morrowind.bsa` for base game archives.
- `encoding=win1252` for Western European text encoding.
- Commented-out entries for Tribunal and Bloodmoon expansions.

The bootstrap runs after IDBFS sync but before `ConfigurationManager` loads configuration, ensuring the config file is available on the virtual filesystem. On subsequent sessions, the existing config (persisted via IDBFS) is preserved.

## Material Controller Uniform Sync

NIF material animation controllers now mirror `osg::Material` changes to `omw_FrontMaterial` uniforms on every frame under Emscripten builds:

- **AlphaController** (`components/nifosg/controller.cpp`): After updating `osg::Material::setDiffuse()` alpha, calls `GLES3Uniforms::applyMaterial()` to sync the uniform struct.
- **MaterialColorController** (`components/nifosg/controller.cpp`): After updating ambient/diffuse/specular/emissive via `osg::Material`, calls `GLES3Uniforms::applyMaterial()` to sync.
- **Sky updaters** (`apps/openmw/mwrender/skyutil.cpp`): `SunUpdater`, `AtmosphereUpdater`, `CloudUpdater`, `SunFlashCallback`, and `SunGlareCallback` all mirror material changes as uniforms.

## Per-Object Texture Matrix Uniform Binding

The `omw_TextureMatrixN` uniforms are now populated from `osg::TexMat` state attributes at every point where texture matrices are set:

- **UVController** (`components/nifosg/controller.cpp`): After setting animated UV TexMat, calls `applyTextureMatrix()` for each texture unit.
- **Terrain materials** (`components/terrain/material.cpp`): Layer tile TexMat and blendmap TexMat are mirrored as uniforms on both shader and non-shader paths.
- **NIF loader** (`components/nifosg/nifloader.cpp`): Static TexMat for NiTextureEffect UV transforms is mirrored.
- **Sky cloud updater** (`apps/openmw/mwrender/skyutil.cpp`): Cloud scrolling TexMat is mirrored as uniform.

## PostProcessor WebGL 2.0 Compatibility

The post-processor (`apps/openmw/mwrender/postprocessor.cpp`) now has Emscripten guards:

- **`glDisablei`**: Nullified (like Android) since WebGL 2.0 lacks indexed blend state.
- **UBO**: Disabled under Emscripten; WebGL 2.0 UBO support differs from GLSL 330 assumptions.
- **GL header**: Uses `<GLES3/gl3.h>` instead of `<SDL_opengl_glext.h>`; `GL_DEPTH_STENCIL_EXT` aliased to `GL_DEPTH_STENCIL`.
- **`GL_LIGHTING`**: Fixed-function lighting mode skipped (unavailable in GLES3).

## File Picker Improvements

The browser data upload (`apps/openmw/wasmfilepicker.cpp`, `files/wasm/openmw_shell.html`) now provides:

- **Two-phase upload**: Directory is first scanned to enumerate all files and compute total size, then files are uploaded with progress tracking.
- **Chunked file reading**: Large files (>8 MB) are read in chunks using `File.slice()` and streamed to the Emscripten FS via `FS.write()`, reducing peak memory usage.
- **Progress callbacks**: JavaScript hooks `__openmwOnUploadPhase` and `__openmwOnUploadProgress` report scanning/uploading phases and per-file/byte progress.
- **C++ progress tracking**: `openmw_wasm_report_upload_progress()` exported function updates file/byte counters accessible via `getUploadedFileCount()`/`getUploadedByteCount()`.
- **UI progress bar**: HTML shell shows a dedicated upload progress bar with file count, byte count, and percentage during data loading.
- **Graceful cancellation**: `AbortError` from the directory picker is caught and handled without error messaging.
- **Periodic yield**: `setTimeout(0)` yield every 50 files prevents the browser from becoming unresponsive during large uploads.

## WebGL 2.0 Rendering Compatibility

Several desktop-only OpenGL features have been guarded or disabled for WebGL 2.0 compatibility:

- **`GL_DEPTH_CLAMP`**: Guarded with `#ifndef __EMSCRIPTEN__` in water rendering (`apps/openmw/mwrender/water.cpp`). WebGL 2.0 does not support depth clamping; the water surface renders without it.
- **`GL_RGBA16F` FBO attachments**: Ripple simulation textures (`apps/openmw/mwrender/ripples.cpp`) fall back to `GL_RGBA8` on Emscripten builds. WebGL 2.0 requires `EXT_color_buffer_float` for half-float render targets, which is not universally available.
- **MyGUI fixed-function pipeline**: The MyGUI draw implementation (`components/myguiplatform/myguirendermanager.cpp`) has been ported from legacy fixed-function calls (`glVertexPointer`, `glColorPointer`, `glTexCoordPointer`, `glEnableClientState`) to modern generic vertex attributes (`glVertexAttribPointer`, `glEnableVertexAttribArray`) under `__EMSCRIPTEN__`. The GUI shader (`gui.vert`/`gui.frag`) is automatically transformed to GLES 3.00 by the existing shader transform pass.
- **`GL_LIGHTING` / `GL_TEXTURE_2D` modes**: Guarded in MyGUI state setup; these fixed-function modes are unavailable in GLES3.
- **`SDL_SetWindowIcon`**: Skipped on Emscripten (canvas windows don't have native icons).

## Audio System Emscripten Guards

The OpenAL output (`apps/openmw/mwsound/openaloutput.cpp`) has been adapted for Emscripten:

- **EFX (Effects Extension)**: Disabled on Emscripten (`ALC.EXT_EFX = false`). Emscripten's OpenAL maps to the Web Audio API, which does not support OpenAL EFX extensions (reverb, filters, auxiliary effect slots).
- **HRTF**: Disabled on Emscripten (`ALC.SOFT_HRTF = false`). Web Audio handles spatialization natively.
- **`StreamThread`**: When building without pthreads (`__EMSCRIPTEN__` && `!__EMSCRIPTEN_PTHREADS__`), the background streaming thread is replaced with a synchronous `processAll()` method called from `finishUpdate()` on the main thread.
- **`DefaultDeviceThread`**: Entirely excluded on Emscripten (`#ifndef __EMSCRIPTEN__`). Audio device routing is handled by the browser.
- **Device reopen / event callbacks**: `alcReopenDeviceSOFT`, `alEventControlSOFT`, and `alEventCallbackSOFT` are skipped on Emscripten; disconnect detection and device switching are browser-managed.

## Threading Guards for Non-Pthread Builds

When building for Emscripten without pthreads (`__EMSCRIPTEN__` && `!__EMSCRIPTEN_PTHREADS__`), the following subsystems fall back to single-threaded operation:

- **Physics** (`apps/openmw/mwphysics/mtphysics.cpp`): `detectLockingPolicy()` returns `NoLocks`, forcing single-threaded physics with 0 worker threads.
- **Lua worker** (`apps/openmw/mwlua/worker.cpp`): Thread creation is guarded; updates run synchronously on the main thread.
- **Work queue** (`apps/openmw/engine.cpp`): Created with 0 worker threads; preloading and screenshots run synchronously.
- **NavMesh updater** (`components/detournavigator/asyncnavmeshupdater.cpp`): Thread creation for both `AsyncNavMeshUpdater` and `DbWorker` is guarded; no background threads are spawned.

## Browser Input & Pointer Lock

The HTML shell (`files/wasm/openmw_shell.html`) now includes a "Click to Play" overlay that appears when the canvas is shown. Browsers require a user gesture (click) before pointer lock can be engaged. The overlay:

1. Appears after game data is loaded and the canvas becomes visible.
2. On click, hides itself, focuses the canvas, and requests pointer lock.
3. SDL2's Emscripten port then handles subsequent pointer lock requests automatically via `SDL_SetRelativeMouseMode`.

## CI / WASM Build Job

A GitLab CI job (`Emscripten_WASM`) has been added to `.gitlab-ci.yml`:

- Runs on `ubuntu:24.04` with the Emscripten SDK.
- Executes `CI/before_script.wasm.sh` to install Emscripten, build dependencies, and configure CMake.
- Builds the WASM target and produces `openmw.html`, `openmw.js`, `openmw.wasm` artifacts.
- Triggered on merge requests (when relevant files change), protected branch pushes, or manually.
- Caches the Emscripten SDK and cross-compiled dependencies across runs.

## Exported Functions

The `EXPORTED_FUNCTIONS` list in `CMakeLists.txt` now includes `_openmw_wasm_report_upload_progress` alongside `_main`, `_openmw_wasm_notify_data_ready`, `_openmw_wasm_is_data_ready`, and `_openmw_wasm_get_data_path`.

## Remaining Work
- **GLSL ES 3.00 shader testing**: The automatic transformation covers all major patterns but needs end-to-end testing with actual shader compilation in WebGL 2.0 to catch edge cases.
- **Audio decoder**: Verify FFmpeg/audio decoding works under Emscripten or provide fallback.
- **Testing and profiling**: End-to-end testing in Chrome with actual Morrowind data, performance profiling and optimization.
- **End-to-end WASM build validation**: Run the `Emscripten_WASM` CI job to verify all dependencies compile and link.

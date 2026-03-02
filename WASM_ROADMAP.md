# OpenMW WebAssembly Port — Roadmap & Status

This document tracks the effort to port OpenMW to WebAssembly (WASM) so that
Morrowind is playable in modern web browsers (Chrome, Firefox, Edge) without
installing any software.

---

## Current Status

> **Early-stage port. The engine compiles and links. In-browser rendering is
> not yet validated end-to-end with real Morrowind data.**

| Area | Status |
|---|---|
| Toolchain / CMake | ✅ Complete |
| Dependency cross-compilation | ✅ Complete |
| Main loop refactoring | ✅ Complete |
| Persistent storage (IDBFS) | ✅ Complete |
| In-browser data loading (File System Access API) | ✅ Complete |
| WebGL 2.0 rendering context | ✅ Complete |
| GLSL ES 3.00 shader transform | ✅ Complete |
| Fixed-function lighting → shader fallback | ✅ Complete |
| MyGUI GLES3 port | ✅ Complete |
| Audio (OpenAL → Web Audio) | ✅ Complete |
| Single-threaded fallback for all subsystems | ✅ Complete |
| Pthread / Web Worker support | ✅ Complete (opt-in) |
| HTML shell (UI, progress, console) | ✅ Complete |
| CI build job | ✅ Complete |
| End-to-end browser testing | ⏳ Pending |
| Performance profiling & optimization | ⏳ Pending |

---

## Remaining Work

1. **End-to-end browser testing** — Load real Morrowind data in Chrome/Firefox,
   verify the main menu renders, and confirm save/load round-trips work.
2. **Performance profiling** — Profile frame time, memory usage, and asset load
   times; optimize hot paths for 60 fps gameplay.
3. **Pthread validation** — Verify the pthread/SharedArrayBuffer path works on
   a properly configured cross-origin isolated host.
4. **Hosting & deployment** — Document (or automate) the required server headers
   (`COOP`/`COEP`) and produce a deployable artifact package.
5. **Post-processor / HDR** — `GL_R8` luminance fallback reduces HDR precision;
   evaluate whether `EXT_color_buffer_float` can be requested conditionally.
6. **Mod/extension compatibility** — Verify common mods work through the browser
   file picker and IDBFS pipeline.

---

## Completed Milestones

### 1. Toolchain & Build System
- CMake detects Emscripten toolchain and auto-enables `OPENMW_IS_EMSCRIPTEN`.
- `OPENMW_EXPERIMENTAL_WASM=ON` switch excludes desktop-only tools (launcher,
  OpenMW-CS, importers) and forces standard Lua (`USE_LUAJIT=OFF`).
- Opt-out switch `OPENMW_EXPERIMENTAL_WASM_PTHREADS=OFF` for single-threaded
  bring-up on hosts without cross-origin isolation.
- All `OPENMW_USE_SYSTEM_*` options forced `OFF`; FetchContent manages
  sub-dependencies uniformly.
- Emscripten port flags (`-sUSE_SDL=2`, `-sUSE_WEBGL2=1`, `-sFULL_ES3=1`,
  `-sALLOW_MEMORY_GROWTH=1`, etc.) injected globally so FetchContent
  sub-builds pick them up automatically.
- Dummy CMake imported targets for SDL2, OpenAL, ZLIB, and OpenGL keep
  existing `find_package()` / `target_link_libraries()` calls working.
- Initial memory: 256 MB; stack size: 5 MB.
- Exported C functions for JS interop: `_main`,
  `_openmw_wasm_notify_data_ready`, `_openmw_wasm_is_data_ready`,
  `_openmw_wasm_get_data_path`, `_openmw_wasm_report_upload_progress`.
- Emscripten runtime methods bundled: `UTF8ToString`, `ccall`, `cwrap`.

### 2. Dependency Cross-Compilation

#### Dependencies provided by Emscripten ports (no build needed)
| Dependency | Port flag | Notes |
|---|---|---|
| SDL2 | `-sUSE_SDL=2` | Window, input, audio device |
| zlib | `-sUSE_ZLIB=1` | Compression |
| libpng | `-sUSE_LIBPNG=1` | PNG image loading |
| libjpeg | `-sUSE_LIBJPEG=1` | JPEG image loading |
| FreeType | `-sUSE_FREETYPE=1` | Font rendering |
| OpenAL | `-lopenal` | 3D audio via Web Audio API |

#### Dependencies built by `CI/build_wasm_deps.sh`
| Dependency | Version | Notes |
|---|---|---|
| Lua | 5.4.7 | Standard Lua (LuaJIT cannot target WASM) |
| LZ4 | 1.9.4 | Static library via `emcc` |
| Boost | 1.84.0 | `program_options` + `iostreams` |
| FFmpeg | 6.1.2 | Minimal codec set (Bink, Vorbis, MP3, etc.) |
| ICU | 70.1 | Host-build for tools, cross-compiled for WASM |

#### Dependencies built by FetchContent
| Dependency | Version | Emscripten adaptations |
|---|---|---|
| Bullet Physics | 3.17 | Double precision ON; multithreading OFF |
| OpenSceneGraph | 3.6 (OpenMW fork) | GLES3 profile forced; GL1/GL2 disabled |
| MyGUI | 3.4.3 | OSG render system 4; static build |
| RecastNavigation | (OpenMW fork) | Static build, no changes needed |
| SQLite3 | 3.41.1 | Amalgamation, no changes needed |
| yaml-cpp | 0.8.0+ | Static build, no changes needed |

Full orchestration: `CI/before_script.wasm.sh` installs Emscripten SDK,
pre-fetches ports, runs `build_wasm_deps.sh`, and configures CMake.

### 3. Main Loop Refactoring
- Game loop body extracted into `Engine::runMainLoopIteration()`.
- Browser-driven `emscripten_set_main_loop_arg()` calls one iteration per
  `requestAnimationFrame`; cancels cleanly on engine quit.
- Post-loop shutdown/persistence in `Engine::shutdownAfterMainLoop()` shared
  between desktop and WASM paths.

### 4. Persistent Storage (IDBFS)
- IDBFS mounted at `/persistent` (override: `OPENMW_WASM_PERSISTENT_ROOT`).
- `HOME` and XDG paths redirected into the mount; config/saves survive reloads.
- XDG subdirectories pre-created before `ConfigurationManager` runs.
- Startup and shutdown syncs; periodic background sync (default 15 s,
  tunable via `OPENMW_WASM_PERSISTENT_SYNC_INTERVAL_MS`; `<=0` disables).
- `visibilitychange` / `pagehide` hooks trigger sync on tab hide/page unload.
- Overlapping sync requests coalesced to avoid IDBFS race conditions.
- FS/IDBFS availability checked at startup with clear console diagnostics.
- Pthread builds warn in the browser console when cross-origin isolation
  headers are missing.

### 5. In-Browser Data Loading
- `wasmfilepicker.hpp/cpp` (`apps/openmw/`) provides a File System Access API
  bridge (`__openmwPickDataDirectory()`).
- **Two-phase upload**: directory scan → enumeration + total size; then
  per-file upload with progress callbacks.
- **Chunked reads**: files > 8 MB read in slices via `File.slice()` /
  `FS.write()` to limit peak memory.
- Progress hooks: `__openmwOnUploadPhase`, `__openmwOnUploadProgress`;
  C++ counters via `openmw_wasm_report_upload_progress()`.
- `setTimeout(0)` yield every 50 files prevents UI freeze on large uploads.
- Cancel handling: returns `true` on success, `false` on `AbortError` (user
  cancel); re-throws on other errors. Shell restores the button and status
  message on cancel.
- Files uploaded into virtual FS at `/gamedata`.
- Auto-generated `openmw.cfg` at first run:
  - `data=/gamedata`, `content=Morrowind.esm`, `fallback-archive=Morrowind.bsa`
  - `encoding=win1252`; commented-out Tribunal/Bloodmoon entries.

### 6. Graphics & Rendering

#### WebGL 2.0 context
- SDL `SDL_GL_SetAttribute` requests OpenGL ES 3.0 (WebGL 2.0).
- MSAA, stereo, fullscreen, debug GL context, and high-DPI hints skipped.
- `SDL_SetWindowIcon` skipped (canvas windows have no native icon).
- Window created with `SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE`.

#### WASM performance defaults (applied at startup)
- Shadows disabled; draw distance capped at 4096; MSAA = 0.
- Reverse-z depth buffer disabled.
- Fixed-function (FFP) lighting marked unsupported; engine falls back to
  `PerObjectUniform` or `SingleUBO` shader-based lighting.

#### WebGL 2.0 compatibility guards
| Feature | Action |
|---|---|
| `GL_DEPTH_CLAMP` | Guarded (`water.cpp`, `shadertechnique.cpp`) |
| `osg::ClipControl` | Guarded in shadow technique |
| `GL_RGBA16F` FBO (ripples) | Remapped to `GL_RGBA8` |
| `GL_R16F` (luminance) | Remapped to `GL_R8` |
| `glDisablei` | Nullified (post-processor) |
| UBO in post-processor | Disabled |
| `GL_LIGHTING` / `GL_NORMALIZE` | Guarded in all affected render files |
| Compute shaders (ripples) | Disabled |
| `GL_LIGHTING` in MyGUI | Guarded |
| `SDL_GetWindowGammaRamp` | Guarded |

#### GLSL ES 3.00 shader transformation
`shadermanager.cpp` runs `transformShaderToGLES3()` automatically for all
shaders under `__EMSCRIPTEN__`. Transformations include:

- Version: `#version 120/330` → `#version 300 es` + precision qualifiers
- Qualifiers: `varying`/`attribute` → `in`/`out`
- Vertex builtins: `gl_Vertex/Normal/Color/MultiTexCoordN` → `osg_*`
- Matrix builtins: `gl_ModelViewMatrix` etc. → `osg_*`; inverse via GLSL built-in
- Texture matrices: `gl_TextureMatrix[N]` → `omw_TextureMatrixN`
- Material/fog/light model: `gl_FrontMaterial.*` → `omw_FrontMaterial.*`, etc.
- Fragment output: `gl_FragData[N]` → `layout(location=N) out vec4 omw_FragDataN`
- Texture lookups: `texture2D`/`textureCube`/etc. → `texture`
- Removed: desktop-only extensions, `#pragma import_defines`

GLES3 preambles prepended to vertex/fragment shaders declare OSG attributes,
matrix uniforms, and custom struct uniforms.

#### GLES3 fixed-function uniform providers (`components/sceneutil/gles3uniforms.hpp/cpp`)
C++ functions mirror GL fixed-function state as uniforms each frame:
`applyMaterial()`, `applyFog()`, `applyTextureMatrix()`, `applyLightModel()`,
`applyAllDefaults()`. Integration points: root scene, NIF loader, fog updater,
NIF animation controllers, sky updaters, terrain materials, UV controllers.

#### MyGUI GLES3 port
`myguirendermanager.cpp` uses generic vertex attributes
(`glVertexAttribPointer`) instead of legacy client arrays for Emscripten.
GUI shaders transformed by the existing GLSL ES 3.00 pass.

### 7. Audio (OpenAL → Web Audio)
- EFX (reverb, filters) disabled; Web Audio handles spatialization natively.
- HRTF disabled.
- `StreamThread`: replaced with synchronous `processAll()` in non-pthread builds.
- `DefaultDeviceThread`: excluded entirely; browser manages audio routing.
- `alcReopenDeviceSOFT` / `alEventControlSOFT`: skipped; browser manages device switching.

### 8. Multithreading & Single-Threaded Fallbacks

#### With pthreads (`-DOPENMW_EXPERIMENTAL_WASM_PTHREADS=ON`)
Requires server headers:
```
Cross-Origin-Opener-Policy: same-origin
Cross-Origin-Embedder-Policy: require-corp
```
Startup warns in the browser console if these are missing.

#### Without pthreads (single-threaded fallback)
| Subsystem | Fallback |
|---|---|
| Physics (`mtphysics.cpp`) | `NoLocks` policy; 0 worker threads |
| Lua worker | Updates run synchronously on main thread |
| Work queue (`engine.cpp`) | 0 worker threads; preloading synchronous |
| NavMesh / DbWorker | No background threads spawned |
| Audio streaming | Synchronous `processAll()` on main thread |
| FFmpeg (audio + video) | `thread_count=1, thread_type=0` before `avcodec_open2` |
| Video player parse/video threads | Skipped; `mVideoEnded=true` immediately |
| Menu background video | `playVideo()` skipped; solid black fill |
| `WorkThread` (`workqueue.cpp`) | `std::thread` construction guarded |

### 9. Platform Adaptations
- **Crash catcher**: Excluded (`crashcatcher.cpp` not compiled; header guard makes
  `crashCatcherInstall` a no-op).
- **`EmscriptenPath`** (`components/files/emscriptenpath.hpp/cpp`): XDG-compatible
  paths under IDBFS (`/persistent/home/.config`, `/persistent/home/.local/share`,
  `/tmp`). Integrated as platform target in `fixedpath.hpp`.
- **Thread priority**: `setCurrentThreadIdlePriority()` is a no-op; logs a message.
- **`std::async` data loading**: Replaced with synchronous `mWorld->loadData()`.
- **Colored console output**: Always `false` in browser; `isatty` bypassed.
- **SDL error dialog**: Shown unconditionally (no TTY check needed).

### 10. HTML Shell (`files/wasm/openmw_shell.html`)
- Loading progress bar during engine init.
- "Select Morrowind Data Folder" button (File System Access API).
- Upload progress bar with file count, byte count, and percentage.
- "Click to Play" overlay — browsers require a user gesture before pointer lock.
  On click: hides overlay, focuses canvas, requests pointer lock; SDL2 handles
  subsequent `SDL_SetRelativeMouseMode` calls automatically.
- Console output overlay (toggle with tilde key).
- Responsive canvas layout.

### 11. CI Build Job (`.gitlab-ci.yml`)
- Job name: `Emscripten_WASM`; base image: `ubuntu:24.04`.
- Executes `CI/before_script.wasm.sh` end-to-end.
- Produces artifacts: `openmw.html`, `openmw.js`, `openmw.wasm`.
- Triggered on merge requests (relevant file changes), protected branch pushes,
  or manually.
- Emscripten SDK and cross-compiled dependency directories cached across runs.

---

## Original Planning Reference

The sections below document the original porting plan for historical context.
Most items are now implemented (see the status table above).

### Toolchain and Build System
The primary compiler for WebAssembly from C++ is [Emscripten](https://emscripten.org/).
- **CMake Integration:** Modify `CMakeLists.txt` files to support Emscripten.
- **Target Architecture:** Add a WASM target alongside Windows, Linux, macOS, Android.
- **Emscripten Flags:** `-s USE_SDL=2`, `-s USE_WEBGL2=1`, `-s FULL_ES3=1`,
  `-s ALLOW_MEMORY_GROWTH=1`, `-s USE_PTHREADS=1`, etc.

### Main Loop
Web browsers do not allow blocking infinite loops. The body of
`while (!mViewer->done() && !mStateManager->hasQuitRequest())` in `Engine::go()`
must be extracted and driven by `emscripten_set_main_loop()` / `requestAnimationFrame`.
✅ Implemented as `Engine::runMainLoopIteration()`.

### Graphics and Rendering
Target WebGL 2.0 (OpenGL ES 3.0). Ensure all GLSL shaders are GLSL ES 3.00
compatible. Desktop OpenGL extensions unavailable in WebGL need fallbacks.
✅ Implemented via automatic shader transform and compatibility guards.

### File System and Assets
Browsers cannot read the local filesystem directly. Options:
- **`--preload-file`** for small assets.
- **File System Access API + IndexedDB** for large Morrowind data files.
✅ Implemented via `wasmfilepicker` and IDBFS.

### Multithreading
OpenMW uses background threads for physics, resource loading, and paging.
Web Workers + `SharedArrayBuffer` (`COOP`/`COEP` headers required).
✅ Implemented with full single-threaded fallback for all subsystems.

### Audio
Emscripten's OpenAL implementation maps to the Web Audio API.
✅ Implemented with EFX/HRTF disabled and streaming thread fallback.

### Input and Controls
SDL2 through Emscripten handles keyboard and mouse. Pointer Lock API requires
a prior user gesture.
✅ Implemented via "Click to Play" overlay in the HTML shell.

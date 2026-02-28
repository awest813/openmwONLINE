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
- Refactors the desktop game loop body into `Engine::runMainLoopIteration(...)` so it can be reused by a browser-driven frame callback.
- Adds an Emscripten-specific `emscripten_set_main_loop_arg(...)` integration that executes one `runMainLoopIteration(...)` per browser frame and cancels cleanly when the engine requests quit.
- Consolidates post-loop shutdown/persistence in `Engine::shutdownAfterMainLoop()` so desktop and WASM paths share the same cleanup behavior.
- Bootstraps an IDBFS mount at `/persistent` for Emscripten builds, redirects HOME/XDG paths into that mount, and triggers startup/shutdown syncs so browser sessions can persist configs/saves across reloads.
- Adds a pthread-hosting diagnostic in Emscripten builds: when compiled with pthread support, startup now warns in the browser console if the page is not cross-origin isolated (missing COOP/COEP headers).
- Hardens Emscripten persistence bootstrap/shutdown scripts with FS/IDBFS availability checks and pre-creates XDG config/data directories under `/persistent/home` before path redirection.

This is still bootstrap-level support: additional dependency and runtime refactoring work is required before OpenMW is playable in-browser.

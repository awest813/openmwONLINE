# Emscripten WASM build configuration helpers for OpenMW
# Include this file after the Emscripten toolchain file is set up:
#   cmake -DCMAKE_TOOLCHAIN_FILE=$EMSDK/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake
#         -C cmake/emscripten-wasm.cmake ..

# Force WASM build
set(OPENMW_EXPERIMENTAL_WASM ON CACHE BOOL "Enable experimental WebAssembly build" FORCE)

# Disable tools that don't apply to browser builds
set(BUILD_LAUNCHER OFF CACHE BOOL "" FORCE)
set(BUILD_WIZARD OFF CACHE BOOL "" FORCE)
set(BUILD_MWINIIMPORTER OFF CACHE BOOL "" FORCE)
set(BUILD_OPENCS OFF CACHE BOOL "" FORCE)
set(BUILD_ESSIMPORTER OFF CACHE BOOL "" FORCE)
set(BUILD_BSATOOL OFF CACHE BOOL "" FORCE)
set(BUILD_ESMTOOL OFF CACHE BOOL "" FORCE)
set(BUILD_NIFTEST OFF CACHE BOOL "" FORCE)
set(BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
set(BUILD_NAVMESHTOOL OFF CACHE BOOL "" FORCE)
set(BUILD_BULLETOBJECTTOOL OFF CACHE BOOL "" FORCE)
set(BUILD_COMPONENTS_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_OPENMW_TESTS OFF CACHE BOOL "" FORCE)

# Use standard Lua (LuaJIT uses native assembly)
set(USE_LUAJIT OFF CACHE BOOL "" FORCE)

# Disable Qt-dependent features
set(BUILD_OPENCS_TESTS OFF CACHE BOOL "" FORCE)

# Emscripten provides these via ports
set(USE_SYSTEM_TINYXML OFF CACHE BOOL "" FORCE)

# Emscripten has built-in zlib support via -sUSE_ZLIB=1
# LZ4 must be cross-compiled with Emscripten separately.
# zlib: Emscripten port available, enable with -sUSE_ZLIB=1 or emcmake
# Boost: Build with Emscripten. Only program_options and iostreams are needed.
#   emcmake cmake .. && emmake make
#   Or use the b2 build system with toolset=emscripten
# OSG: Must be cross-compiled with Emscripten. Key configuration:
#   -DOSG_GL3_AVAILABLE=OFF
#   -DOSG_GLES2_AVAILABLE=OFF  
#   -DOSG_GLES3_AVAILABLE=ON
#   -DOSG_GL_DISPLAYLISTS_AVAILABLE=OFF
#   -DOSG_GL_FIXED_FUNCTION_AVAILABLE=OFF
#   -DOSG_GL_VERTEX_FUNCS_AVAILABLE=OFF
#   -DDYNAMIC_OPENSCENEGRAPH=OFF
#   -DDYNAMIC_OPENTHREADS=OFF
# Bullet: Cross-compile with Emscripten:
#   emcmake cmake -DBUILD_BULLET2_DEMOS=OFF -DBUILD_UNIT_TESTS=OFF
#                 -DBUILD_EXTRAS=OFF -DBUILD_CPU_DEMOS=OFF ..
# MyGUI: Cross-compile with Emscripten:
#   emcmake cmake -DMYGUI_RENDERSYSTEM=1 -DMYGUI_BUILD_DEMOS=OFF
#                 -DMYGUI_BUILD_TOOLS=OFF -DMYGUI_BUILD_PLUGINS=OFF ..
# Lua: Compiles cleanly with Emscripten:
#   emcc -c src/*.c && emar rcs liblua.a *.o

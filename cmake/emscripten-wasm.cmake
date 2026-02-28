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

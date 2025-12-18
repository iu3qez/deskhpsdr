# CMake Toolchain File for Cross-Compiling to Windows from Linux
# Usage: cmake -DCMAKE_TOOLCHAIN_FILE=Windows/mingw-toolchain.cmake ..

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# Specify the cross-compiler
set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)

# Target environment
set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32)

# Search for programs in the build host directories
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)

# Search for libraries and headers in the target directories
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# pkg-config for cross-compilation
# Include both lib and lib64 for pkgconfig paths
set(ENV{PKG_CONFIG_PATH} "/usr/x86_64-w64-mingw32/lib/pkgconfig:/usr/x86_64-w64-mingw32/lib64/pkgconfig")
set(ENV{PKG_CONFIG_LIBDIR} "/usr/x86_64-w64-mingw32/lib/pkgconfig:/usr/x86_64-w64-mingw32/lib64/pkgconfig")
# NOTE: Do NOT set PKG_CONFIG_SYSROOT_DIR - it causes path duplication

# Use standard pkg-config with MinGW paths
set(PKG_CONFIG_EXECUTABLE /usr/bin/pkg-config)

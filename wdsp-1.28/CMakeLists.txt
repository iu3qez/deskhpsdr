cmake_minimum_required(VERSION 3.10)

# WDSP Library

# Sources
file(GLOB WDSP_SOURCES "*.c")

# Exclude linux_port.c on Windows
if(WIN32)
    list(REMOVE_ITEM WDSP_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/linux_port.c")
endif()

# Include directories specific to WDSP if any (it seems self-contained mostly, but needs fftw3)
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFTW3 REQUIRED fftw3)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${FFTW3_INCLUDE_DIRS}
)

add_library(wdsp STATIC ${WDSP_SOURCES})

target_link_libraries(wdsp 
    ${FFTW3_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)

if(WIN32)
    # Link against standard Windows libraries if needed by internal WDSP logic
    # linux_port.c replacement logic might need adjustment or is handled by native windows calls in wdsp code implicitly?
    # Actually checking linux_port.h again, on WIN32 it includes Windows.h, process.h etc.
endif()

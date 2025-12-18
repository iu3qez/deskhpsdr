# CMakeLists.txt for piHPSDR Windows Cross-Compilation
# This is ONLY for Windows builds. Linux/macOS continue to use Makefile.

cmake_minimum_required(VERSION 3.16)
project(pihpsdr C)

# Check if we're cross-compiling for Windows or building natively on Windows
if(NOT CMAKE_CROSSCOMPILING AND NOT WIN32)
    message(FATAL_ERROR "This CMakeLists.txt is only for Windows builds. Use Makefile for Linux/macOS native builds.\nFor cross-compilation, use: cmake -DCMAKE_TOOLCHAIN_FILE=Windows/mingw-toolchain.cmake ..")
endif()

# Force Windows target when cross-compiling
if(CMAKE_CROSSCOMPILING)
    set(WIN32 TRUE)
    set(MINGW TRUE)
endif()

#######################################################################################
# Build Options
#######################################################################################

option(MIDI "Enable MIDI support" ON)
option(USBOZY "Enable USB OZY support" OFF)
option(SOAPYSDR "Enable SoapySDR support" OFF)
option(STEMLAB "Enable STEMlab/RedPitaya web interface" OFF)
option(TTS "Enable Text-to-Speech" ON)

# These are ALWAYS OFF on Windows
set(GPIO OFF)
set(SATURN OFF)

# Windows always uses PortAudio
set(AUDIO "PORTAUDIO")

#######################################################################################
# Compiler Settings
#######################################################################################

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -Wall -Wextra -Wimplicit-fallthrough")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unused-parameter -Wno-deprecated-declarations")

# Git version info
execute_process(
    COMMAND git describe --abbrev=0 --tags --always --dirty
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
execute_process(
    COMMAND git log --pretty=format:%h -1
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
execute_process(
    COMMAND git show --date=short --format=%ai --name-only
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_DATE_RAW
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
string(REGEX MATCH "^[0-9-]+" GIT_DATE "${GIT_DATE_RAW}")

if(NOT GIT_VERSION)
    set(GIT_VERSION "unknown")
endif()
if(NOT GIT_COMMIT)
    set(GIT_COMMIT "unknown")
endif()
if(NOT GIT_DATE)
    set(GIT_DATE "unknown")
endif()

add_definitions(-DGIT_VERSION="${GIT_VERSION}")
add_definitions(-DGIT_COMMIT="${GIT_COMMIT}")
add_definitions(-DGIT_DATE="${GIT_DATE}")

#######################################################################################
# Find Packages
#######################################################################################

find_package(PkgConfig REQUIRED)

pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
pkg_check_modules(FFTW3 REQUIRED fftw3)
pkg_check_modules(PORTAUDIO REQUIRED portaudio-2.0)
pkg_check_modules(OPENSSL REQUIRED openssl)
pkg_check_modules(CURL REQUIRED libcurl)
pkg_check_modules(JSONC REQUIRED json-c)

# Add library directories for all packages (especially lib64 for OpenSSL)
link_directories(
    ${GTK3_LIBRARY_DIRS}
    ${FFTW3_LIBRARY_DIRS}
    ${PORTAUDIO_LIBRARY_DIRS}
    ${OPENSSL_LIBRARY_DIRS}
    ${CURL_LIBRARY_DIRS}
    ${JSONC_LIBRARY_DIRS}
)

if(USBOZY)
    pkg_check_modules(LIBUSB REQUIRED libusb-1.0)
endif()

if(SOAPYSDR)
    find_library(SOAPYSDR_LIB SoapySDR)
endif()

if(STEMLAB)
    pkg_check_modules(CURL REQUIRED libcurl)
endif()

#######################################################################################
# Include Directories
#######################################################################################

include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/wdsp-1.28
    ${CMAKE_SOURCE_DIR}/Windows
    ${CMAKE_SOURCE_DIR}/libtelnet
    ${CMAKE_SOURCE_DIR}/libsolar
    ${GTK3_INCLUDE_DIRS}
    ${FFTW3_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIRS}
    ${PORTAUDIO_INCLUDE_DIRS}
    ${CURL_INCLUDE_DIRS}
    ${JSONC_INCLUDE_DIRS}
)

if(USBOZY)
    include_directories(${LIBUSB_INCLUDE_DIRS})
endif()

if(STEMLAB)
    include_directories(${CURL_INCLUDE_DIRS})
endif()

#######################################################################################
# Definitions
#######################################################################################

add_definitions(-DPORTAUDIO)
add_definitions(-D_WIN32)

if(MIDI)
    add_definitions(-DMIDI)
endif()

if(USBOZY)
    add_definitions(-DUSBOZY)
endif()

if(SOAPYSDR)
    add_definitions(-DSOAPYSDR)
endif()

if(STEMLAB)
    add_definitions(-DSTEMLAB_DISCOVERY)
endif()

if(TTS)
    add_definitions(-DTTS)
endif()

#######################################################################################
# Source Files - Core (excluding platform-specific)
#######################################################################################

set(CORE_SOURCES
    src/about_menu.c
    src/actions.c
    src/action_dialog.c
    src/agc_menu.c
    src/ant_menu.c
    src/appearance.c
    src/band.c
    src/band_menu.c
    src/bandstack_menu.c
    # bootloader.c excluded - requires libpcap (WinPcap/Npcap on Windows)
    src/configure.c
    src/css.c
    src/cw_menu.c
    src/cwramp.c
    src/discovered.c
    src/discovery.c
    src/display_menu.c
    src/diversity_menu.c
    src/dxcluster.c
    src/encoder_menu.c
    src/equalizer_menu.c
    src/exit_menu.c
    src/ext.c
    src/extras_menu.c
    src/fft_menu.c
    src/filter.c
    src/filter_menu.c
    src/iambic.c
    src/led.c
    src/main.c
    src/message.c
    src/meter.c
    src/meter_menu.c
    src/mode.c
    src/mode_menu.c
    src/new_discovery.c
    src/new_menu.c
    src/new_protocol.c
    src/noise_menu.c
    src/oc_menu.c
    src/old_discovery.c
    src/old_protocol.c
    src/pa_menu.c
    src/property.c
    src/protocols.c
    src/ps_menu.c
    src/radio.c
    src/radio_menu.c
    src/receiver.c
    src/rigctl.c
    src/rigctl_menu.c
    src/rx_menu.c
    src/rx_panadapter.c
    src/screen_menu.c
    src/sintab.c
    src/sliders.c
    src/startup.c
    src/store.c
    src/store_menu.c
    src/switch_menu.c
    src/tci.c
    src/toolbar.c
    src/toolbar_menu.c
    # toolset.c excluded - requires libxml2 (via libsolar)
    src/transmitter.c
    src/tx_menu.c
    src/tx_panadapter.c
    # udplistener.c excluded - uses fork(), execl(), posix_openpt() (not portable)
    # udpsender.c excluded - standalone tool with its own main()
    src/version.c
    src/vfo.c
    src/vfo_menu.c
    src/vox.c
    src/vox_menu.c
    src/waterfall.c
    src/xvtr_menu.c
    src/zoompan.c
)

#######################################################################################
# Windows Compatibility Layer
#######################################################################################

set(WINDOWS_SOURCES
    Windows/windows_compat.c
    Windows/windows_stubs.c
)

#######################################################################################
# libtelnet (for DX Cluster support)
#######################################################################################

set(LIBTELNET_SOURCES
    libtelnet/libtelnet.c
)

#######################################################################################
# libsolar (for solar calculations - greyline map)
#######################################################################################

# libsolar excluded - requires libxml2 (only used by toolset.c)
# set(LIBSOLAR_SOURCES
#     libsolar/solar.c
# )

#######################################################################################
# Audio - PortAudio for Windows
#######################################################################################

set(AUDIO_SOURCES
    src/portaudio.c
)

#######################################################################################
# Optional: MIDI
#######################################################################################

if(MIDI)
    set(MIDI_SOURCES
        Windows/windows_midi.c
        src/midi2.c
        src/midi3.c
        src/midi_menu.c
    )
endif()

#######################################################################################
# Optional: USB OZY
#######################################################################################

if(USBOZY)
    set(USBOZY_SOURCES
        src/ozyio.c
    )
endif()

#######################################################################################
# Optional: SoapySDR
#######################################################################################

if(SOAPYSDR)
    set(SOAPYSDR_SOURCES
        src/soapy_discovery.c
        src/soapy_protocol.c
    )
endif()

#######################################################################################
# Optional: STEMlab
#######################################################################################

if(STEMLAB)
    set(STEMLAB_SOURCES
        src/stemlab_discovery.c
    )
endif()

#######################################################################################
# Optional: TTS
#######################################################################################

if(TTS)
    set(TTS_SOURCES
        src/tts.c
    )
endif()

#######################################################################################
# WDSP Library
#######################################################################################

# Build WDSP as a static library
file(GLOB WDSP_SOURCES wdsp-1.28/*.c)
# Exclude linux_port.c on Windows - WDSP is natively Windows
list(FILTER WDSP_SOURCES EXCLUDE REGEX "linux_port\\.c$")

add_library(wdsp STATIC ${WDSP_SOURCES})
target_include_directories(wdsp PRIVATE ${CMAKE_SOURCE_DIR}/wdsp-1.28 ${FFTW3_INCLUDE_DIRS})
target_compile_definitions(wdsp PRIVATE _WIN32)

#######################################################################################
# Git Version Information
#######################################################################################

execute_process(
    COMMAND git describe --abbrev=0 --tags
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(NOT GIT_VERSION)
    set(GIT_VERSION "unknown")
endif()

execute_process(
    COMMAND git log -1 --format=%h
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(NOT GIT_COMMIT)
    set(GIT_COMMIT "unknown")
endif()

execute_process(
    COMMAND git rev-parse --abbrev-ref HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_BRANCH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(NOT GIT_BRANCH)
    set(GIT_BRANCH "unknown")
endif()

string(TIMESTAMP GIT_DATE "%Y-%m-%d %H:%M:%S")

#######################################################################################
# Main Executable
#######################################################################################

add_executable(pihpsdr
    ${CORE_SOURCES}
    ${WINDOWS_SOURCES}
    ${LIBTELNET_SOURCES}
    ${AUDIO_SOURCES}
    ${MIDI_SOURCES}
    ${USBOZY_SOURCES}
    ${SOAPYSDR_SOURCES}
    ${STEMLAB_SOURCES}
    ${TTS_SOURCES}
)

#######################################################################################
# Link Libraries
#######################################################################################

target_compile_definitions(pihpsdr PRIVATE
    GIT_VERSION="${GIT_VERSION}"
    GIT_COMMIT="${GIT_COMMIT}"
    GIT_BRANCH="${GIT_BRANCH}"
    GIT_DATE="${GIT_DATE}"
)

target_link_libraries(pihpsdr
    wdsp
    ${GTK3_LIBRARIES}
    ${FFTW3_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    ${PORTAUDIO_LIBRARIES}
    ${CURL_LIBRARIES}
    ${JSONC_LIBRARIES}
    ws2_32      # Winsock2
    winmm       # Windows Multimedia (MIDI)
    iphlpapi    # IP Helper API (getifaddrs replacement)
    crypt32     # Windows Crypto API (for OpenSSL)
    gdi32       # GDI (for OpenSSL)
    avrt        # Audio Video Runtime (for WDSP)
    pthread     # POSIX threads (provides clock_gettime/nanosleep)
)

if(USBOZY)
    target_link_libraries(pihpsdr ${LIBUSB_LIBRARIES})
endif()

if(SOAPYSDR)
    target_link_libraries(pihpsdr ${SOAPYSDR_LIB})
endif()

if(STEMLAB)
    target_link_libraries(pihpsdr ${CURL_LIBRARIES})
endif()

#######################################################################################
# Installation
#######################################################################################

install(TARGETS pihpsdr DESTINATION bin)

#######################################################################################
# Cross-compilation toolchain hint
#######################################################################################

# To cross-compile from Linux:
# cmake -DCMAKE_TOOLCHAIN_FILE=Windows/mingw-toolchain.cmake ..

cmake_minimum_required(VERSION 3.10)
project(deskhpsdr C)

# Set C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Dependencies
find_package(PkgConfig REQUIRED)

pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
pkg_check_modules(FFTW3 REQUIRED fftw3)
pkg_check_modules(OPENSSL REQUIRED openssl)
pkg_check_modules(LIBCURL REQUIRED libcurl)
pkg_check_modules(LIBXML2 REQUIRED libxml-2.0)
pkg_check_modules(JSONC REQUIRED json-c)

# Platform specific checks
if(WIN32)
    message(STATUS "Building for Windows (MSYS2/MinGW)")
    pkg_check_modules(PORTAUDIO REQUIRED portaudio-2.0)
    add_definitions(-DPORTAUDIO)
    # Windows specific definitions
    add_definitions(-D_WIN32_WINNT=0x0600) # Target Windows Vista or later
    add_definitions(-DCURL_STATICLIB) # Use static curl library
    set(PLATFORM_LIBS ws2_32 winmm iphlpapi avrt curl)
elseif(APPLE)
    message(STATUS "Building for macOS")
    pkg_check_modules(PORTAUDIO REQUIRED portaudio-2.0)
    add_definitions(-DPORTAUDIO -DPA_USE_COREAUDIO)
    find_library(COREMIDI_LIB CoreMIDI)
    find_library(FOUNDATION_LIB Foundation)
    find_library(AVFOUNDATION_LIB AVFoundation)
    find_library(IOKIT_LIB IOKit)
    find_library(COCOA_LIB Cocoa)
    find_library(WEBKIT_LIB WebKit)
    set(PLATFORM_LIBS ${COREMIDI_LIB} ${FOUNDATION_LIB} ${AVFOUNDATION_LIB} ${IOKIT_LIB} ${COCOA_LIB} ${WEBKIT_LIB})
else() # Linux
    message(STATUS "Building for Linux")
    # Default to PulseAudio on Linux unless ALSA is explicitly requested
    if(AUDIO STREQUAL "ALSA")
        pkg_check_modules(ALSA REQUIRED alsa)
        add_definitions(-DALSA)
        set(AUDIO_LIBS ${ALSA_LIBRARIES})
    else()
        pkg_check_modules(PULSE REQUIRED libpulse-simple libpulse libpulse-mainloop-glib)
        add_definitions(-DPULSEAUDIO)
        set(AUDIO_LIBS ${PULSE_LIBRARIES})
    endif()
    
    # GPIO support (Linux only)
    pkg_check_modules(LIBGPIOD libgpiod)
    if(LIBGPIOD_FOUND)
        add_definitions(-DGPIO)
        include_directories(${LIBGPIOD_INCLUDE_DIRS})
        set(PLATFORM_LIBS ${LIBGPIOD_LIBRARIES})
    endif()
    
    set(PLATFORM_LIBS ${PLATFORM_LIBS} rt m)
endif()

# Threading
find_package(Threads REQUIRED)

# Include directories
include_directories(
    src
    wdsp-1.28
    libsolar
    libtelnet
    ${GTK3_INCLUDE_DIRS}
    ${FFTW3_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIRS}
    ${LIBCURL_INCLUDE_DIRS}
    ${LIBXML2_INCLUDE_DIRS}
    ${JSONC_INCLUDE_DIRS}
)

if(PORTAUDIO_FOUND)
    include_directories(${PORTAUDIO_INCLUDE_DIRS})
endif()

# Link directories for libraries
link_directories(
    ${OPENSSL_LIBRARY_DIRS}
    ${LIBCURL_LIBRARY_DIRS}
    ${LIBXML2_LIBRARY_DIRS}
    ${JSONC_LIBRARY_DIRS}
    ${FFTW3_LIBRARY_DIRS}
)

if(PORTAUDIO_FOUND)
    link_directories(${PORTAUDIO_LIBRARY_DIRS})
endif()

# Subdirectories for libraries
add_subdirectory(wdsp-1.28)
add_subdirectory(libsolar)
add_subdirectory(libtelnet)

# Source files
file(GLOB SOURCES "src/*.c")

# Exclude standalone tools/simulators from the main application build
list(REMOVE_ITEM SOURCES 
    "${CMAKE_SOURCE_DIR}/src/bootloader.c"
    "${CMAKE_SOURCE_DIR}/src/bootldrsim.c"
    "${CMAKE_SOURCE_DIR}/src/hpsdrsim.c"
    "${CMAKE_SOURCE_DIR}/src/newhpsdrsim.c"
    "${CMAKE_SOURCE_DIR}/src/udplistener.c"
    "${CMAKE_SOURCE_DIR}/src/udpsender.c"
    "${CMAKE_SOURCE_DIR}/src/gpio.c"
)

# MIDI sources
if(WIN32)
    # Windows MIDI support using Windows Multimedia API
    list(REMOVE_ITEM SOURCES
        "${CMAKE_SOURCE_DIR}/src/alsa_midi.c"
        "${CMAKE_SOURCE_DIR}/src/mac_midi.c"
    )
    # Add Windows-specific implementations
    list(APPEND SOURCES
        "${CMAKE_SOURCE_DIR}/src/windows_midi.c"
        "${CMAKE_SOURCE_DIR}/src/gpio_stubs.c"
    )

    # Exclude Linux-specific hardware support on Windows
    list(REMOVE_ITEM SOURCES
        "${CMAKE_SOURCE_DIR}/src/soapy_discovery.c"
        "${CMAKE_SOURCE_DIR}/src/soapy_protocol.c"
        "${CMAKE_SOURCE_DIR}/src/ozyio.c"              # USB hardware (libusb)
        "${CMAKE_SOURCE_DIR}/src/saturn_menu.c"        # Saturn hardware
        "${CMAKE_SOURCE_DIR}/src/saturndrivers.c"
        "${CMAKE_SOURCE_DIR}/src/saturnmain.c"
        "${CMAKE_SOURCE_DIR}/src/saturnregisters.c"
        "${CMAKE_SOURCE_DIR}/src/saturnserver.c"
    )
elseif(APPLE)
     list(REMOVE_ITEM SOURCES "${CMAKE_SOURCE_DIR}/src/alsa_midi.c")
else()
     list(REMOVE_ITEM SOURCES "${CMAKE_SOURCE_DIR}/src/mac_midi.c")
endif()

# Audio backend sources
if(WIN32 OR APPLE)
    list(REMOVE_ITEM SOURCES 
        "${CMAKE_SOURCE_DIR}/src/pulseaudio.c" 
        "${CMAKE_SOURCE_DIR}/src/audio.c" # ALSA
    )
else()
    # Linux
    if(AUDIO STREQUAL "ALSA")
         list(REMOVE_ITEM SOURCES "${CMAKE_SOURCE_DIR}/src/pulseaudio.c" "${CMAKE_SOURCE_DIR}/src/portaudio.c")
    else()
         list(REMOVE_ITEM SOURCES "${CMAKE_SOURCE_DIR}/src/audio.c" "${CMAKE_SOURCE_DIR}/src/portaudio.c")
    endif()
endif()


# Executable
add_executable(deskhpsdr ${SOURCES})

# Git Version info
find_package(Git)
if(GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --abbrev=0 --tags --always --dirty
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    execute_process(
        COMMAND ${GIT_EXECUTABLE} log -1 --format=%h
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_COMMIT
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --abbrev-ref HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_BRANCH
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    execute_process(
        COMMAND ${GIT_EXECUTABLE} show --no-pager --date=short --format=%ai --name-only
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_DATE_FULL
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    # Extract just the date part from the first line
    string(REGEX REPLACE "\n.*" "" GIT_DATE "${GIT_DATE_FULL}")
    string(SUBSTRING "${GIT_DATE}" 0 10 GIT_DATE)

    target_compile_definitions(deskhpsdr PRIVATE 
        GIT_VERSION="${GIT_VERSION}"
        GIT_COMMIT="${GIT_COMMIT}"
        GIT_BRANCH="${GIT_BRANCH}"
        GIT_DATE="${GIT_DATE}"
    )
endif()

# Link libraries
target_link_libraries(deskhpsdr
    wdsp
    solar
    telnet
    ${GTK3_LIBRARIES}
    ${FFTW3_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    ${LIBCURL_LIBRARIES}
    ${LIBXML2_LIBRARIES}
    ${JSONC_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
    ${PLATFORM_LIBS}
)

if(PORTAUDIO_FOUND)
    target_link_libraries(deskhpsdr ${PORTAUDIO_LIBRARIES})
endif()

if(AUDIO_LIBS)
    target_link_libraries(deskhpsdr ${AUDIO_LIBS})
endif()

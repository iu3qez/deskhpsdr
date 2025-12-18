# piHPSDR Windows Porting - Technical Notes

This document summarizes the actual changes made to port piHPSDR to Windows using MinGW cross-compilation from Linux.

## Build System

### CMake (Primary Build System)
- **CMakeLists.txt**: Complete CMake configuration for Windows cross-compilation
  - Excludes: GPIO, SATURN, simulators, ALSA (Linux-only)
  - Uses: PortAudio for audio, WinMM for MIDI
  - Builds WDSP as static library (excluding `linux_port.c`)
  - Links with Windows libraries: `ws2_32`, `winmm`, `iphlpapi`, `crypt32`, `gdi32`, `avrt`, `pthread`

- **Windows/mingw-toolchain.cmake**: Cross-compilation toolchain file
  - Compiler: `x86_64-w64-mingw32-gcc`
  - Configures pkg-config paths for MinGW libraries

### Cross-Compilation Command
```bash
mkdir build-windows && cd build-windows
cmake -DCMAKE_TOOLCHAIN_FILE=../Windows/mingw-toolchain.cmake ..
cmake --build . -j$(nproc)
```

## Windows Compatibility Layer

### New Files Created

#### Headers (POSIX Compatibility Shims)
Located in `Windows/` directory, these provide POSIX-compatible interfaces:
- `windows_compat.h` - Main compatibility header with:
  - Socket API wrappers (Winsock2 → POSIX)
  - `close()` → `closesocket()`
  - `sleep()`/`usleep()` → `Sleep()`
  - POSIX semaphore API (`sem_t`, `sem_init`, `sem_wait`, etc.)
  - POSIX thread stubs (using Windows CRITICAL_SECTION for mutexes)
  - Error code mappings (`EWOULDBLOCK`, `EAGAIN`, etc.)
  - Socket option compatibility (`SO_REUSEPORT` → `SO_REUSEADDR`)
  - Andromeda hardware stubs

- POSIX header shims (redirect to MinGW or provide stubs):
  - `arpa/inet.h`, `netinet/in.h`, `netinet/ip.h`, `netinet/tcp.h`
  - `sys/socket.h`, `sys/select.h`, `sys/ioctl.h`, `sys/mman.h`, `sys/utsname.h`
  - `net/if.h`, `net/if_arp.h`, `netdb.h`
  - `ifaddrs.h` - Network interface enumeration structures
  - `poll.h`, `fcntl.h`, `unistd.h`
  - `pthread.h`, `semaphore.h`
  - `termios.h` - Serial port structures (stubs)
  - `pwd.h` - User database structures
  - `endian.h` - Byte order macros

#### Source Files
- **windows_compat.c** (~630 lines): POSIX function implementations
  - `sem_init()`, `sem_destroy()`, `sem_wait()`, `sem_trywait()`, `sem_post()`, `sem_close()`
  - `pthread_create()`, `pthread_join()`, `pthread_detach()`
  - `pthread_mutex_init()`, `pthread_mutex_lock()`, `pthread_mutex_trylock()`, `pthread_mutex_unlock()`
  - `fcntl()` - F_GETFL/F_SETFL for socket non-blocking mode
  - `bcopy()`, `realpath()`, `inet_aton()`, `index()`, `rindex()`
  - `uname()` - System information using Windows API
  - `getifaddrs()` / `freeifaddrs()` - Network interface enumeration via `GetAdaptersAddresses()`
  - `tcgetattr()`, `tcsetattr()`, `tcflush()` - Serial port stubs
  - `getpwuid()`, `getuid()`, `getgid()` - User info using `GetUserNameA()`
  - `andromeda_execute_button()`, `andromeda_execute_encoder()` - Hardware stubs

- **windows_midi.c** (~250 lines): Windows MIDI implementation
  - Uses Windows Multimedia API (`winmm.dll`)
  - Replaces `alsa_midi.c` on Windows
  - Functions: `register_midi_device()`, `close_midi_device()`, `get_midi_devices()`, `get_midi_device_name()`

## Source Code Modifications

### Macro/Enum Conflicts Fixed
Several identifiers conflicted with Windows headers:

| Original | Renamed To | Reason |
|----------|-----------|--------|
| `SNB` | `selection_SNB` | Conflicts with Windows `SNB` macro |
| `RELATIVE` | `MODE_RELATIVE` | Conflicts with Windows definition |
| `ABSOLUTE` | `MODE_ABSOLUTE` | Conflicts with Windows definition |
| `PRESSED` | `MODE_PRESSED` | Conflicts with Windows definition |

Files affected: `src/actions.c`, `src/actions.h`

### WDSP Library Changes
- **wdsp/comm.h**: Changed `#include <Windows.h>` → `#include <windows.h>` (case-sensitive for Linux cross-compile)
- **wdsp/wdsp.h**:
  - Added `#include <windows.h>` for Windows builds
  - Wrapped `DWORD`, `__stdcall`, `LPCRITICAL_SECTION` definitions with `#ifndef _WIN32`

### PortAudio (src/portaudio.c)
- Use `defaultLowOutputLatency` instead of 0.0 for output latency (fixes Windows audio issues)

### Other Modified Files
Most source files were modified to:
1. Include `windows_compat.h` when `_WIN32` is defined
2. Replace direct socket calls with compatibility macros
3. Handle Windows-specific path separators

## Files Excluded from Windows Build
These files are Linux/Raspberry Pi specific and excluded via CMakeLists.txt:

- `src/gpio.c`, `src/gpio.h` - Raspberry Pi GPIO
- `src/saturnmain.c`, `src/saturnserver.c` - SATURN/G2 XDMA driver
- `src/saturn_menu.c` - SATURN menu
- `src/andromeda.c` - Andromeda hardware (I2C)
- `src/i2c.c` - I2C communication
- `src/audio.c` - ALSA audio (replaced by portaudio.c)
- `src/pulseaudio.c` - PulseAudio (Linux only)
- `src/alsa_midi.c` - ALSA MIDI (replaced by windows_midi.c)
- `wdsp/linux_port.c` - WDSP Linux-specific code
- Simulator files: `src/hpsdrsim.c`, `src/newhpsdrsim.c`, `src/bootldrsim.c`

## Packaging

### package.sh Script
Creates distribution package with:
- `pihpsdr.exe` - Main executable
- All required MinGW DLLs (~49 MB)
- GTK3 runtime files (icons, schemas, loaders)
- `setup-env.bat` - Launcher with environment setup

### Required DLLs
- GCC Runtime: `libgcc_s_seh-1.dll`, `libstdc++-6.dll`, `libwinpthread-1.dll`
- GTK3 Stack: gtk, gdk, glib, gio, gobject, cairo, pango, harfbuzz, etc.
- Audio: PortAudio

### GTK Runtime Files
- Pixbuf loaders for PNG, JPEG, BMP, GIF, ICO, SVG
- Adwaita icon theme (subset)
- GSettings schemas

## Development Environment

### DevContainer Configuration
- Based on Ubuntu with MinGW-w64 cross-compiler
- Pre-installed dependencies for cross-compilation
- Files: `.devcontainer/Dockerfile`, `.devcontainer/devcontainer.json`

## Known Limitations

1. **No GPIO** - Raspberry Pi specific
2. **No SATURN/G2** - Requires Linux XDMA kernel driver
3. **No Andromeda** - Hardware panel requires I2C
4. **Serial ports** - Basic stubs only (termios)
5. **Netmask detection** - Uses default 255.255.255.0 (MinGW compatibility)

## Compatibility Matrix

| Feature | Linux | macOS | Windows |
|---------|-------|-------|---------|
| GUI | GTK3 | GTK3 | GTK3 |
| Audio | ALSA/Pulse | PortAudio | PortAudio |
| MIDI | ALSA | CoreMIDI | WinMM |
| Network (P1/P2) | ✅ | ✅ | ✅ |
| GPIO | ✅ | ❌ | ❌ |
| SATURN/G2 | ✅ | ❌ | ❌ |
| USB (OZY) | ✅ | ✅ | ✅ |
| SoapySDR | ✅ | ✅ | Optional |
| STEMlab | ✅ | ✅ | ✅ |


## Building ##
cmake -DCMAKE_TOOLCHAIN_FILE=../Windows/mingw-toolchain.cmake ..

# Windows Packaging Notes

## Overview
This document describes the cross-compilation and packaging process for piHPSDR on Windows.

## Build Environment
- **Host OS**: Linux (Ubuntu-based devcontainer)
- **Compiler**: MinGW-w64 GCC 10 (x86_64-w64-mingw32-gcc)
- **Target**: Windows x86_64

## Scripts

### 1. build-docker.sh
Cross-compiles piHPSDR for Windows from Linux.

**Usage:**
```bash
./Windows/build-docker.sh
```

**Features:**
- Interactive menu for full/partial builds
- Automatically configures MinGW toolchain
- Builds WDSP library and piHPSDR
- Sets correct environment variables for Windows detection

### 2. package.sh
Creates a distribution package with all required runtime dependencies.

**Usage:**
```bash
./Windows/package.sh
```

**Output:**
- `dist-windows/` - Complete distribution directory
- All necessary DLLs automatically collected
- GTK3 runtime files (icons, schemas, loaders)
- Configuration files (GTK settings, pixbuf cache)
- Batch launcher script with environment setup

## Runtime Dependencies

### GCC Runtime (Essential)
Located in `/usr/lib/gcc/x86_64-w64-mingw32/10-win32/`:
- `libgcc_s_seh-1.dll` - GCC exception handling
- `libstdc++-6.dll` - C++ standard library (26 MB)
- `libwinpthread-1.dll` - POSIX threads implementation

### MinGW Libraries
Located in `/usr/x86_64-w64-mingw32/bin/`:
- GTK3 stack (gtk, gdk, glib, gio, gobject, etc.)
- Graphics libraries (cairo, pixman, pango, harfbuzz)
- Image libraries (png, freetype, fontconfig)
- Other utilities (intl, pcre2, ffi, etc.)

### GTK3 Runtime Files

**Pixbuf Loaders** (`lib/gdk-pixbuf-2.0/2.10.0/loaders/`):
- PNG, JPEG, BMP, GIF, ICO, SVG support
- `loaders.cache` file for loader registration

**Icons** (`share/icons/Adwaita/`):
- Essential icon sizes: 16x16, 22x22, 24x24, 32x32, 48x48
- Minimal subset to reduce package size

**Schemas** (`share/glib-2.0/schemas/`):
- GLib/GTK settings schemas

**Configuration** (`etc/gtk-3.0/settings.ini`):
- GTK theme and appearance settings

## Package Structure

```
dist-windows/
├── pihpsdr.exe              # Main executable (11 MB)
├── setup-env.bat            # Launcher with environment setup (RECOMMENDED)
├── README.txt               # User instructions
├── *.dll                    # 37 runtime libraries (~49 MB)
├── etc/
│   └── gtk-3.0/
│       └── settings.ini     # GTK configuration
├── lib/
│   └── gdk-pixbuf-2.0/
│       └── 2.10.0/
│           ├── loaders/     # Image format plugins
│           └── loaders.cache
└── share/
    ├── glib-2.0/schemas/    # GSettings schemas
    └── icons/Adwaita/       # Icon theme

Total size: ~60 MB uncompressed, ~21 MB in ZIP
```

## Distribution

**Creating ZIP:**
```bash
zip -r pihpsdr-windows.zip dist-windows/
```

**Testing on Windows:**
1. Extract `pihpsdr-windows.zip` to any folder
2. Double-click `setup-env.bat` (recommended) or `pihpsdr.exe`

## Known Issues Fixed

1. **libgcc_s_seh-1.dll missing**
   - Added GCC runtime library search path
   - Explicitly copies essential GCC DLLs

2. **GTK images not loading**
   - Generated `loaders.cache` with correct paths
   - Added GDK_PIXBUF environment variables in launcher

3. **Theme/icon issues**
   - Created GTK settings.ini
   - Included Adwaita icon theme subset
   - Set GTK_DATA_PREFIX in launcher

## Code Modifications for Windows Compatibility

### wdsp/comm.h
- Changed `#include <Windows.h>` → `#include <windows.h>` (case-sensitive on Linux)

### Windows/windows_midi.c
- Changed `#include <mmeapi.h>` → `#include <mmsystem.h>` (MinGW compatibility)

### Windows/windows_compat.c
- Removed `OnLinkPrefixLength` usage (not in MinGW GCC 10)
- Used default netmask for compatibility

### src/ozyio.c
- Added Windows-specific `filePath()` implementation
- Uses `GetModuleFileNameA()` instead of `/proc/self/exe`

### src/startup.c
- Changed configuration directory from `%USERPROFILE%/Documents/piHPSDR` to `%LOCALAPPDATA%/piHPSDR`
- Uses `getenv("LOCALAPPDATA")` to get `C:\Users\username\AppData\Local\piHPSDR`
- Avoids cloud sync issues with OneDrive/Google Drive
- Follows Windows best practices for application data storage
- Fallback to Documents if `LOCALAPPDATA` is not set (rare case)

## Build System Changes

### Windows/build-docker.sh
- Exports `CC=x86_64-w64-mingw32-gcc`
- Sets `OS=Windows_NT` for Makefile detection
- Configures PKG_CONFIG paths for MinGW
- Adds `-I./Windows` to include compatibility headers

### Makefile (no changes)
- Existing Windows detection works with `OS=Windows_NT`
- Correctly selects Windows MIDI implementation
- Uses PortAudio backend as configured

## Launcher Script (setup-env.bat)

Sets environment variables for GTK runtime:
- `GDK_PIXBUF_MODULEDIR` - Pixbuf loaders path
- `GDK_PIXBUF_MODULE_FILE` - Loaders cache file
- `GTK_DATA_PREFIX` - GTK data files location
- `GTK_EXE_PREFIX` - GTK executable location

Uses relative paths (`%~dp0`) to work from any location.

## Future Improvements

Possible enhancements:
- NSIS installer for easier installation
- Automatic DLL stripping to reduce size
- Include only used icon theme assets
- Optional: build static executable (complex with GTK)

# Windows Packaging Guide for deskHPSDR

This document explains how to create a Windows distribution package for deskHPSDR.

## Prerequisites

- Docker container with MinGW-w64 cross-compilation environment (see `.devcontainer/Dockerfile`)
- Built executable in `build/deskhpsdr.exe`

## Quick Start

```bash
cd /workspaces/deskhpsdr
./package-windows.sh
```

This will create:
- `dist/deskhpsdr-windows/` - Complete portable distribution
- `dist/deskhpsdr-windows-YYYYMMDD.zip` - Ready-to-distribute archive

## What the Script Does

The `package-windows.sh` script automatically:

1. **Collects DLL dependencies** - Recursively analyzes the executable and all its dependencies to find required DLLs
2. **Copies runtime files** - Includes GTK themes, icons, pixbuf loaders, and GLib schemas
3. **Creates portable structure** - Organizes everything in a Windows-compatible layout
4. **Generates README** - Adds user documentation
5. **Creates ZIP archive** - Packages everything for distribution

## Package Contents

### Executables and Libraries (39 DLLs + exe)
- `deskhpsdr.exe` (15 MB) - Main application
- GTK3 stack: gtk, gdk, gdk-pixbuf, cairo, pango, harfbuzz
- GLib stack: glib, gobject, gio, gmodule
- Compiler runtime: libgcc_s_seh-1, libstdc++-6, libwinpthread-1
- System libraries: libxml2, zlib, libpng, freetype, fontconfig, etc.
- Accessibility: libgailutil-3-0 (for GTK accessibility support)

### Runtime Data Files
- **GDK-Pixbuf loaders** (14 DLLs): Image format support for PNG, JPEG, GIF, SVG, BMP, ICO, etc.
- **Icon themes**: Adwaita (7.6 MB) and hicolor (1.7 MB)
- **GLib schemas** (6 XML files): GTK settings and configuration schemas
- **GTK themes**: Default and Emacs themes

### Total Size
- Uncompressed: ~75 MB
- Compressed (ZIP): ~30-40 MB

## Testing with Wine

You can test the Windows package on Linux using Wine:

```bash
cd dist/deskhpsdr-windows
wine ./deskhpsdr.exe
```

### Expected Wine Warnings (Non-Critical)

These warnings are normal and can be ignored:

1. **"Failed to load module gail:atk-bridge"**
   - GTK accessibility bridge module
   - Not available in MSYS2 MinGW packages
   - Application works fine without it
   - Solution: `libgailutil-3-0.dll` is included for basic accessibility support

2. **"ntlm_auth was not found"**
   - Windows NTLM authentication
   - Not needed for this application
   - Can be ignored

3. **GDK backend warnings**
   - Display system compatibility messages
   - Wine's X11/Wayland bridge
   - Application should render correctly despite warnings

## Customization

### Reducing Package Size

Edit `package-windows.sh` to exclude optional components:

1. **Icon theme hicolor** - Comment out in `copy_icons()` (saves 1.7 MB)
2. **GTK themes** - Comment out `copy_themes()` (saves ~1 MB)
3. **Some pixbuf loaders** - Keep only PNG/JPEG in `copy_pixbuf_loaders()`

### Adding Dependencies

If you add new libraries to the build:

1. Rebuild the executable with new dependencies
2. Run `package-windows.sh` - it will automatically detect new DLLs
3. Check the output for any "DLL not found" warnings
4. If a DLL is in a non-standard location, update the search paths in the script

## Troubleshooting

### "DLL not found" warnings during packaging

Check if the DLL is:
- A Windows system DLL (add to `SYSTEM_DLLS` list if so)
- In a custom location (add search path to `copy_dlls()` function)
- Missing from the build environment (install the MinGW package)

### Application fails to start on Windows

1. Check that all DLLs are in the same directory as the .exe
2. Verify icon themes are present in `share/icons/`
3. Ensure GLib schemas are in `share/glib-2.0/schemas/`
4. Try running from Command Prompt to see error messages

### Missing icons or themes

The application needs:
- `share/icons/Adwaita/` - Primary icon theme
- `share/icons/hicolor/` - Fallback theme
- Both must be present for proper rendering

### GTK warnings about schemas

If you see schema-related warnings, ensure:
- Schema XML files are in `share/glib-2.0/schemas/`
- The directory structure is preserved
- GTK will compile them at runtime if needed

## Integration with Build System

You can add the packaging script as a CMake target:

```cmake
# Add to CMakeLists.txt
add_custom_target(package-windows
    COMMAND ${CMAKE_SOURCE_DIR}/package-windows.sh
    DEPENDS deskhpsdr
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Creating Windows distribution package"
)
```

Then run:
```bash
cmake --build build --target package-windows
```

## Docker Container Requirements

The packaging script requires these tools in the container:
- `x86_64-w64-mingw32-objdump` - DLL dependency analysis
- `zip` - Archive creation (added to `.devcontainer/Dockerfile`)
- Standard Unix tools: `find`, `grep`, `awk`, `sed`

## Distribution

The generated ZIP file can be:
1. Uploaded to GitHub Releases
2. Distributed via website download
3. Shared directly with users

Users simply:
1. Extract the ZIP file
2. Run `deskhpsdr.exe`
3. No installation required

## Notes

- The package is fully portable - no registry entries or installation needed
- Configuration files are stored in the user's Windows profile
- The application can run from USB drives or any directory
- All dependencies are self-contained in the distribution

## Maintenance

When updating dependencies:
1. Update the Docker container packages
2. Rebuild the executable
3. Run `package-windows.sh` to regenerate the package
4. Test with Wine or on a Windows VM
5. Check for new DLL warnings and update exclusion lists if needed

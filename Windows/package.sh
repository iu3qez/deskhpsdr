#!/bin/bash

#####################################################
#
# Package piHPSDR for Windows distribution
# Collects all necessary DLLs and creates a dist/ folder
#
######################################################

echo "piHPSDR Windows Packaging Script"
echo "================================="

# Check if pihpsdr.exe exists (support both old Makefile and new CMake build locations)
EXE_PATH=""
if [ -f "pihpsdr.exe" ]; then
    EXE_PATH="pihpsdr.exe"
elif [ -f "build-windows/pihpsdr.exe" ]; then
    EXE_PATH="build-windows/pihpsdr.exe"
else
    echo "Error: pihpsdr.exe not found."
    echo "Please build first with one of:"
    echo "  - CMake: cd build-windows && cmake -DCMAKE_TOOLCHAIN_FILE=../Windows/mingw-toolchain.cmake .. && make"
    echo "  - Docker: ./Windows/build-docker.sh"
    exit 1
fi
echo "Found executable: $EXE_PATH"

# Create dist directory
DIST_DIR="dist-windows"
echo "Creating distribution directory: $DIST_DIR"
rm -rf "$DIST_DIR"
mkdir -p "$DIST_DIR"

# Copy the executable
echo "Copying pihpsdr.exe..."
cp "$EXE_PATH" "$DIST_DIR/pihpsdr.exe"

# MinGW base directory
MINGW_PREFIX="/usr/x86_64-w64-mingw32"
MINGW_BIN="$MINGW_PREFIX/bin"
GCC_LIBDIR="/usr/lib/gcc/x86_64-w64-mingw32/10-win32"

echo "Analyzing dependencies with objdump..."

# Function to copy a DLL and its dependencies recursively
copy_dll_deps() {
    local dll_path="$1"
    local dll_name=$(basename "$dll_path")

    # Skip if already copied
    if [ -f "$DIST_DIR/$dll_name" ]; then
        return
    fi

    # Copy the DLL
    if [ -f "$dll_path" ]; then
        cp "$dll_path" "$DIST_DIR/"
        echo "  + $dll_name"
    fi
}

# Get list of DLLs needed by the executable
echo ""
echo "Collecting required DLLs..."

# Use objdump to find dependencies
DEPS=$(x86_64-w64-mingw32-objdump -p "$EXE_PATH" | grep "DLL Name:" | awk '{print $3}')

# Copy each dependency
for dll in $DEPS; do
    # Skip Windows system DLLs
    case $dll in
        KERNEL32.dll|USER32.dll|ADVAPI32.dll|msvcrt.dll|GDI32.dll|SHELL32.dll|ole32.dll|OLEAUT32.dll|WS2_32.dll|WSOCK32.dll|COMDLG32.dll|IMM32.dll|COMCTL32.dll|VERSION.dll|WINMM.dll|CRYPT32.dll|IPHLPAPI.dll|IPHLPAPI.DLL|AVRT.dll|SAPI.dll|uuid.dll|RPCRT4.dll|SETUPAPI.dll|bcrypt.dll|ntdll.dll|DNSAPI.dll|DWrite.dll|HID.DLL|MSIMG32.dll|SHLWAPI.dll|USP10.dll|WINSPOOL.DRV|WLDAP32.dll|comdlg32.dll|dwmapi.dll|gdiplus.dll)
            # Skip Windows system DLLs
            continue
            ;;
    esac

    # Look for the DLL in MinGW directories
    dll_path=""
    if [ -f "$MINGW_BIN/$dll" ]; then
        dll_path="$MINGW_BIN/$dll"
    elif [ -f "$GCC_LIBDIR/$dll" ]; then
        dll_path="$GCC_LIBDIR/$dll"
    elif [ -f "$MINGW_PREFIX/lib/$dll" ]; then
        dll_path="$MINGW_PREFIX/lib/$dll"
    fi

    if [ -n "$dll_path" ]; then
        copy_dll_deps "$dll_path"
    fi
done

# Also copy essential GCC runtime libraries explicitly
echo ""
echo "Adding GCC runtime libraries..."
for gcc_dll in libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll; do
    if [ -f "$GCC_LIBDIR/$gcc_dll" ]; then
        copy_dll_deps "$GCC_LIBDIR/$gcc_dll"
    fi
done

# Copy common missing dependencies that objdump might miss
echo ""
echo "Adding additional common dependencies..."
for extra_dll in libbz2-1.dll libbrotlidec.dll libexpat-1.dll libgraphite2.dll libiconv-2.dll libpangoft2-1.0-0.dll libthai-0.dll; do
    if [ -f "$MINGW_BIN/$extra_dll" ]; then
        copy_dll_deps "$MINGW_BIN/$extra_dll"
    fi
done

# Also recursively check dependencies of copied DLLs
echo ""
echo "Checking dependencies of copied DLLs..."

for copied_dll in "$DIST_DIR"/*.dll; do
    if [ -f "$copied_dll" ]; then
        SUB_DEPS=$(x86_64-w64-mingw32-objdump -p "$copied_dll" 2>/dev/null | grep "DLL Name:" | awk '{print $3}')

        for dll in $SUB_DEPS; do
            # Skip Windows system DLLs
            case $dll in
                KERNEL32.dll|USER32.dll|ADVAPI32.dll|msvcrt.dll|GDI32.dll|SHELL32.dll|ole32.dll|OLEAUT32.dll|WS2_32.dll|WSOCK32.dll|COMDLG32.dll|IMM32.dll|COMCTL32.dll|VERSION.dll|WINMM.dll|CRYPT32.dll|IPHLPAPI.dll|IPHLPAPI.DLL|AVRT.dll|SAPI.dll|uuid.dll|RPCRT4.dll|SETUPAPI.dll|bcrypt.dll|ntdll.dll|DNSAPI.dll|DWrite.dll|HID.DLL|MSIMG32.dll|SHLWAPI.dll|USP10.dll|WINSPOOL.DRV|WLDAP32.dll|comdlg32.dll|dwmapi.dll|gdiplus.dll)
                    continue
                    ;;
            esac

            # Look for the DLL
            dll_path=""
            if [ -f "$MINGW_BIN/$dll" ]; then
                dll_path="$MINGW_BIN/$dll"
            elif [ -f "$GCC_LIBDIR/$dll" ]; then
                dll_path="$GCC_LIBDIR/$dll"
            elif [ -f "$MINGW_PREFIX/lib/$dll" ]; then
                dll_path="$MINGW_PREFIX/lib/$dll"
            fi

            if [ -n "$dll_path" ]; then
                copy_dll_deps "$dll_path"
            fi
        done
    fi
done

# Copy GTK3 data files (icons, themes, etc.)
echo ""
echo "Copying GTK3 runtime files..."

# Create GTK directories
mkdir -p "$DIST_DIR/share/glib-2.0/schemas"
mkdir -p "$DIST_DIR/share/icons"
mkdir -p "$DIST_DIR/lib/gdk-pixbuf-2.0"

# Copy schemas
if [ -d "$MINGW_PREFIX/share/glib-2.0/schemas" ]; then
    cp -r "$MINGW_PREFIX/share/glib-2.0/schemas"/* "$DIST_DIR/share/glib-2.0/schemas/" 2>/dev/null || true
    echo "  + GLib schemas"
fi

# Copy icon theme (Adwaita - minimal)
if [ -d "$MINGW_PREFIX/share/icons/Adwaita" ]; then
    mkdir -p "$DIST_DIR/share/icons/Adwaita"
    cp "$MINGW_PREFIX/share/icons/Adwaita/index.theme" "$DIST_DIR/share/icons/Adwaita/" 2>/dev/null || true
    # Copy only essential icon sizes to save space
    for size in 16x16 22x22 24x24 32x32 48x48; do
        if [ -d "$MINGW_PREFIX/share/icons/Adwaita/$size" ]; then
            cp -r "$MINGW_PREFIX/share/icons/Adwaita/$size" "$DIST_DIR/share/icons/Adwaita/" 2>/dev/null || true
        fi
    done
    echo "  + Adwaita icon theme (minimal)"
fi

# Copy GDK pixbuf loaders
if [ -d "$MINGW_PREFIX/lib/gdk-pixbuf-2.0" ]; then
    cp -r "$MINGW_PREFIX/lib/gdk-pixbuf-2.0"/* "$DIST_DIR/lib/gdk-pixbuf-2.0/" 2>/dev/null || true
    echo "  + GDK pixbuf loaders"

    # Generate loaders.cache file for the dist directory
    # This is critical for GTK to find image loaders
    LOADERS_DIR="$DIST_DIR/lib/gdk-pixbuf-2.0/2.10.0/loaders"
    if [ -d "$LOADERS_DIR" ]; then
        cat > "$DIST_DIR/lib/gdk-pixbuf-2.0/2.10.0/loaders.cache" << 'CACHE_EOF'
# GdkPixbuf Image Loader Modules file
# Automatically generated - DO NOT EDIT

"lib\\gdk-pixbuf-2.0\\2.10.0\\loaders\\libpixbufloader-png.dll"
"png" 5 "gdk-pixbuf" "PNG" "LGPL"
"image/png" ""
"png" ""
"\211PNG\r\n\032\n" "" 100

"lib\\gdk-pixbuf-2.0\\2.10.0\\loaders\\libpixbufloader-jpeg.dll"
"jpeg" 5 "gdk-pixbuf" "JPEG" "LGPL"
"image/jpeg" ""
"jpeg" "jpe" "jpg" ""
"\377\330" "" 100

"lib\\gdk-pixbuf-2.0\\2.10.0\\loaders\\libpixbufloader-bmp.dll"
"bmp" 5 "gdk-pixbuf" "BMP" "LGPL"
"image/bmp" "image/x-bmp" "image/x-MS-bmp" ""
"bmp" ""
"BM" "" 100

"lib\\gdk-pixbuf-2.0\\2.10.0\\loaders\\libpixbufloader-gif.dll"
"gif" 5 "gdk-pixbuf" "GIF" "LGPL"
"image/gif" ""
"gif" ""
"GIF8" "" 100

"lib\\gdk-pixbuf-2.0\\2.10.0\\loaders\\libpixbufloader-ico.dll"
"ico" 5 "gdk-pixbuf" "ICO" "LGPL"
"image/x-icon" "image/x-ico" "image/x-win-bitmap" ""
"ico" "cur" ""
"  \001   " "zz znz" 100
"  \002   " "zz znz" 100

"lib\\gdk-pixbuf-2.0\\2.10.0\\loaders\\libpixbufloader-svg.dll"
"svg" 6 "gdk-pixbuf" "SVG" "LGPL"
"image/svg+xml" "image/svg" "image/svg-xml" ""
"svg" "svgz" "svg.gz" ""
" <svg" "*    " 100
" <!DOCTYPE svg" "*             " 100

CACHE_EOF
        echo "  + GDK pixbuf loaders.cache"
    fi
fi

# Create GTK settings
mkdir -p "$DIST_DIR/etc/gtk-3.0"
cat > "$DIST_DIR/etc/gtk-3.0/settings.ini" << 'SETTINGS_EOF'
[Settings]
gtk-theme-name = Default
gtk-icon-theme-name = Adwaita
gtk-font-name = Segoe UI 10
gtk-cursor-theme-size = 0
gtk-toolbar-style = GTK_TOOLBAR_BOTH
gtk-toolbar-icon-size = GTK_ICON_SIZE_LARGE_TOOLBAR
gtk-button-images = 1
gtk-menu-images = 1
gtk-enable-event-sounds = 1
gtk-enable-input-feedback-sounds = 1
gtk-xft-antialias = 1
gtk-xft-hinting = 1
gtk-xft-hintstyle = hintfull
gtk-xft-rgba = rgb
SETTINGS_EOF
echo "  + GTK3 settings"

# Create gdk-pixbuf environment setup script
cat > "$DIST_DIR/setup-env.bat" << 'BATCH_EOF'
@echo off
REM Setup environment for piHPSDR

REM Set GDK_PIXBUF_MODULEDIR to use relative path
set GDK_PIXBUF_MODULEDIR=%~dp0lib\gdk-pixbuf-2.0\2.10.0\loaders
set GDK_PIXBUF_MODULE_FILE=%~dp0lib\gdk-pixbuf-2.0\2.10.0\loaders.cache

REM Set GTK paths
set GTK_DATA_PREFIX=%~dp0
set GTK_EXE_PREFIX=%~dp0

REM Run piHPSDR
"%~dp0pihpsdr.exe" %*
BATCH_EOF
echo "  + setup-env.bat launcher"

# Create a simple README
cat > "$DIST_DIR/README.txt" << 'EOF'
piHPSDR for Windows
===================

This package contains piHPSDR cross-compiled for Windows using MinGW.

Contents:
- pihpsdr.exe         : Main executable
- setup-env.bat       : Launcher script (RECOMMENDED)
- *.dll               : Required runtime libraries
- share/              : GTK3 runtime data files (icons, schemas)
- lib/                : GTK3 modules (pixbuf loaders)
- etc/                : GTK3 configuration

To run:
1. Extract this entire folder to a location on your Windows machine
2. Double-click setup-env.bat to run (RECOMMENDED)
   OR double-click pihpsdr.exe directly

Note: Keep all files in the same directory structure. The program needs
the DLLs and data files to function properly.

The setup-env.bat script sets up the correct environment variables for
GTK3 to find its resources. Using it is recommended for best compatibility.

Configuration Files:
--------------------
piHPSDR saves all its files in:
  %LOCALAPPDATA%\piHPSDR\
  (typically: C:\Users\<username>\AppData\Local\piHPSDR\)

This directory will contain:

Configuration files (.props):
- Radio settings: Named by MAC address (e.g., 02-03-04-05-06-07.props)
- gpio.props       : GPIO settings
- ipaddr.props     : IP address settings
- protocols.props  : Protocol settings
- remote.props     : Remote settings
- midi.props       : MIDI settings

DSP optimization:
- wdspWisdom00     : FFTW wisdom file (FFT optimization data)
                     Created automatically on first run, speeds up DSP processing

Log files:
- pihpsdr.stdout   : Standard output log
- pihpsdr.stderr   : Error log (useful for troubleshooting)

The .props and wisdom files are created automatically. The wisdom file creation
can take a few minutes on first run while it optimizes FFT sizes up to 262144.

IMPORTANT: Backup the entire %LOCALAPPDATA%\piHPSDR\ folder to preserve
all your configurations and DSP optimizations.

Note: Using %LOCALAPPDATA% (AppData\Local) instead of Documents avoids issues
with cloud sync services (OneDrive, Google Drive) trying to sync binary files
and logs. This is the recommended location for application data on Windows.

For more information, see the main piHPSDR documentation.
EOF

echo "  + README.txt"

# Count files
DLL_COUNT=$(ls -1 "$DIST_DIR"/*.dll 2>/dev/null | wc -l)
TOTAL_SIZE=$(du -sh "$DIST_DIR" | awk '{print $1}')

echo ""
echo "================================================"
echo "Packaging complete!"
echo "================================================"
echo "Distribution directory: $DIST_DIR/"
echo "Executable: pihpsdr.exe"
echo "DLLs copied: $DLL_COUNT"
echo "Total size: $TOTAL_SIZE"
echo ""
echo "To create a ZIP archive:"
echo "  zip -r pihpsdr-windows.zip $DIST_DIR/"
echo ""
echo "To test on Windows:"
echo "  1. Copy the entire '$DIST_DIR' folder to a Windows machine"
echo "  2. Run pihpsdr.exe from inside that folder"
echo ""

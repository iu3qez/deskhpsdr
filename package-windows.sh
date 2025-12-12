#!/bin/bash
#
# package-windows.sh - Script per creare il pacchetto di distribuzione Windows per deskHPSDR
#
# Questo script raccoglie l'eseguibile deskhpsdr.exe e tutte le sue dipendenze
# (DLL GTK3, file di runtime, temi, icone) in un pacchetto distribuibile per Windows.
#
# Utilizzo:
#   ./package-windows.sh
#
# Output:
#   dist/deskhpsdr-windows-YYYYMMDD.zip
#

set -e  # Esci in caso di errore

# Configurazione
MINGW_PREFIX="/usr/x86_64-w64-mingw32"
MINGW_BIN="$MINGW_PREFIX/bin"
MINGW_LIB="$MINGW_PREFIX/lib"
GCC_LIB="/usr/lib/gcc/x86_64-w64-mingw32"
BUILD_DIR="build"
DIST_DIR="dist/deskhpsdr-windows"
EXE_NAME="deskhpsdr.exe"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Colori per output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Lista di DLL di sistema Windows da escludere
SYSTEM_DLLS="KERNEL32|USER32|WS2_32|ADVAPI32|CRYPT32|WINMM|AVRT|IPHLPAPI|WLDAP32|msvcrt|GDI32|ole32|SHELL32|COMCTL32|comdlg32|IMM32|WINSPOOL|dwmapi|uxtheme|SETUPAPI|bcrypt|RPCRT4|SHLWAPI|OLEAUT32|NETAPI32|VERSION|WS2HELP|WSOCK32|SECUR32|USERENV|PSAPI|MPR|DNSAPI|DWrite|HID|MSIMG32|USP10|gdiplus"

# Funzione per stampare messaggi
log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Funzione per raccogliere ricorsivamente le dipendenze DLL
collect_dll_dependencies() {
    local target="$1"
    local dll_list_file="$2"

    # Verifica che il file target esista
    if [ ! -f "$target" ]; then
        log_warn "File not found: $target"
        return
    fi

    # Estrai le DLL importate
    local imports=$(x86_64-w64-mingw32-objdump -p "$target" 2>/dev/null | \
                    grep "DLL Name:" | \
                    awk '{print $3}' | \
                    grep -viE "^(${SYSTEM_DLLS})\.dll$")

    # Per ogni DLL importata
    for dll in $imports; do
        # Converti a lowercase per confronto case-insensitive
        dll_lower=$(echo "$dll" | tr '[:upper:]' '[:lower:]')

        # Se già processata, salta
        if grep -iq "^${dll}$" "$dll_list_file" 2>/dev/null; then
            continue
        fi

        # Aggiungi alla lista
        echo "$dll" >> "$dll_list_file"

        # Cerca la DLL in vari percorsi MinGW
        local dll_path=""
        if [ -f "$MINGW_BIN/$dll" ]; then
            dll_path="$MINGW_BIN/$dll"
        elif [ -f "$MINGW_LIB/$dll" ]; then
            dll_path="$MINGW_LIB/$dll"
        else
            # Cerca in directory GCC (per libgcc, libstdc++, etc.)
            dll_path=$(find "$GCC_LIB" -name "$dll" -type f 2>/dev/null | head -1)
        fi

        # Se trovata, processa ricorsivamente
        if [ -n "$dll_path" ] && [ -f "$dll_path" ]; then
            collect_dll_dependencies "$dll_path" "$dll_list_file"
        fi
    done
}

# Funzione per copiare le DLL nel directory di distribuzione
copy_dlls() {
    local dll_list_file="$1"
    local count=0
    local not_found=0

    log_info "Copying DLL dependencies..."

    while IFS= read -r dll; do
        local dll_path=""

        # Cerca la DLL in vari percorsi
        if [ -f "$MINGW_BIN/$dll" ]; then
            dll_path="$MINGW_BIN/$dll"
        elif [ -f "$MINGW_LIB/$dll" ]; then
            dll_path="$MINGW_LIB/$dll"
        else
            # Cerca in directory GCC
            dll_path=$(find "$GCC_LIB" -name "$dll" -type f 2>/dev/null | head -1)
        fi

        if [ -n "$dll_path" ] && [ -f "$dll_path" ]; then
            cp "$dll_path" "$DIST_DIR/"
            count=$((count + 1))
        else
            log_warn "DLL not found: $dll"
            not_found=$((not_found + 1))
        fi
    done < "$dll_list_file"

    log_info "Copied $count DLL files"
    if [ $not_found -gt 0 ]; then
        log_warn "$not_found DLL(s) not found (likely Windows system DLLs)"
    fi
}

# Funzione per copiare i pixbuf loaders
copy_pixbuf_loaders() {
    log_info "Copying GDK-Pixbuf loaders..."

    local loaders_src="$MINGW_PREFIX/lib/gdk-pixbuf-2.0/2.10.0/loaders"
    local loaders_base="$DIST_DIR/lib/gdk-pixbuf-2.0/2.10.0"
    local loaders_dst="$loaders_base/loaders"

    if [ -d "$loaders_src" ]; then
        mkdir -p "$loaders_dst"

        # Copia solo i file .dll
        cp "$loaders_src"/*.dll "$loaders_dst/" 2>/dev/null || true

        # Genera loaders.cache nel posto corretto (directory padre)
        # Formato richiesto da gdk-pixbuf
        cat > "$loaders_base/loaders.cache" <<'CACHEHEADER'
# GDK-Pixbuf Image Loader Modules file
# Automatically generated for Windows distribution
#
CACHEHEADER

        # PNG loader - essenziale per GTK
        cat >> "$loaders_base/loaders.cache" <<'PNGLOADER'
"lib/gdk-pixbuf-2.0/2.10.0/loaders/libpixbufloader-png.dll"
"png" 2 "gdk-pixbuf" "PNG image format"
"image/png" ""
"png" ""
"\x89PNG\x0d\x0a\x1a\x0a" "" 100

PNGLOADER

        # BMP loader
        cat >> "$loaders_base/loaders.cache" <<'BMPLOADER'
"lib/gdk-pixbuf-2.0/2.10.0/loaders/libpixbufloader-bmp.dll"
"bmp" 2 "gdk-pixbuf" "BMP image format"
"image/bmp" "image/x-bmp" "image/x-MS-bmp" ""
"bmp" ""
"BM" "" 100

BMPLOADER

        # GIF loader
        cat >> "$loaders_base/loaders.cache" <<'GIFLOADER'
"lib/gdk-pixbuf-2.0/2.10.0/loaders/libpixbufloader-gif.dll"
"gif" 6 "gdk-pixbuf" "GIF image format"
"image/gif" ""
"gif" ""
"GIF8" "" 100

GIFLOADER

        # JPEG loader
        cat >> "$loaders_base/loaders.cache" <<'JPEGLOADER'
"lib/gdk-pixbuf-2.0/2.10.0/loaders/libpixbufloader-jpeg.dll"
"jpeg" 2 "gdk-pixbuf" "JPEG image format"
"image/jpeg" ""
"jpeg" "jpe" "jpg" ""
"\xff\xd8" "" 100

JPEGLOADER

        # ICO loader
        cat >> "$loaders_base/loaders.cache" <<'ICOLOADER'
"lib/gdk-pixbuf-2.0/2.10.0/loaders/libpixbufloader-ico.dll"
"ico" 2 "gdk-pixbuf" "Windows icon format"
"image/x-icon" "image/x-ico" "image/x-win-bitmap" ""
"ico" "cur" ""
"\x00\x00\x01\x00" "xxxx" 100
"\x00\x00\x02\x00" "xxxx" 100

ICOLOADER

        # SVG loader
        cat >> "$loaders_base/loaders.cache" <<'SVGLOADER'
"lib/gdk-pixbuf-2.0/2.10.0/loaders/libpixbufloader-svg.dll"
"svg" 2 "gdk-pixbuf" "Scalable Vector Graphics format"
"image/svg+xml" "image/svg" "image/svg-xml" ""
"svg" "svgz" "svg.gz" ""
" <svg" "*   " 100
"<?xml" "*    " 100

SVGLOADER

        # XPM loader
        cat >> "$loaders_base/loaders.cache" <<'XPMLOADER'
"lib/gdk-pixbuf-2.0/2.10.0/loaders/libpixbufloader-xpm.dll"
"xpm" 2 "gdk-pixbuf" "X Pixmap format"
"image/x-xpixmap" ""
"xpm" ""
"/* XPM */" "" 100

XPMLOADER

        # TIFF loader
        cat >> "$loaders_base/loaders.cache" <<'TIFFLOADER'
"lib/gdk-pixbuf-2.0/2.10.0/loaders/libpixbufloader-tiff.dll"
"tiff" 2 "gdk-pixbuf" "TIFF image format"
"image/tiff" ""
"tiff" "tif" ""
"MM\x00*" "" 100
"II*\x00" "" 100

TIFFLOADER

        log_info "Copied $(ls -1 "$loaders_dst"/*.dll 2>/dev/null | wc -l) pixbuf loaders"
        log_info "Created loaders.cache at lib/gdk-pixbuf-2.0/2.10.0/loaders.cache"
    else
        log_warn "Pixbuf loaders directory not found: $loaders_src"
    fi
}

# Funzione per copiare i temi icone
copy_icons() {
    log_info "Copying icon themes..."

    local icons_dst="$DIST_DIR/share/icons"
    mkdir -p "$icons_dst"

    # Copia tema Adwaita (essenziale)
    if [ -d "$MINGW_PREFIX/share/icons/Adwaita" ]; then
        cp -r "$MINGW_PREFIX/share/icons/Adwaita" "$icons_dst/"
        log_info "Copied Adwaita icon theme ($(du -sh "$icons_dst/Adwaita" 2>/dev/null | cut -f1))"
    else
        log_warn "Adwaita icon theme not found"
    fi

    # Copia tema hicolor (fallback)
    if [ -d "$MINGW_PREFIX/share/icons/hicolor" ]; then
        cp -r "$MINGW_PREFIX/share/icons/hicolor" "$icons_dst/"
        log_info "Copied hicolor icon theme ($(du -sh "$icons_dst/hicolor" 2>/dev/null | cut -f1))"
    else
        log_warn "hicolor icon theme not found"
    fi
}

# Funzione per copiare gli schemi GLib
copy_glib_schemas() {
    log_info "Copying GLib schemas..."

    local schemas_src="$MINGW_PREFIX/share/glib-2.0/schemas"
    local schemas_dst="$DIST_DIR/share/glib-2.0/schemas"

    if [ -d "$schemas_src" ]; then
        mkdir -p "$schemas_dst"

        # Copia i file .xml
        cp "$schemas_src"/*.xml "$schemas_dst/" 2>/dev/null || true

        # Se esiste gschemas.compiled, copialo
        if [ -f "$schemas_src/gschemas.compiled" ]; then
            cp "$schemas_src/gschemas.compiled" "$schemas_dst/"
            log_info "Copied compiled GLib schemas"
        else
            log_warn "Pre-compiled schemas not found - copied XML files only"
            log_warn "GTK will compile them at runtime if needed"
        fi

        log_info "Copied $(ls -1 "$schemas_dst"/*.xml 2>/dev/null | wc -l) schema files"
    else
        log_warn "GLib schemas directory not found: $schemas_src"
    fi
}

# Funzione per copiare i temi GTK (opzionale)
copy_themes() {
    log_info "Copying GTK themes..."

    local themes_src="$MINGW_PREFIX/share/themes"
    local themes_dst="$DIST_DIR/share/themes"

    if [ -d "$themes_src" ]; then
        mkdir -p "$themes_dst"

        # Copia solo i temi essenziali per ridurre dimensioni
        for theme in Default Emacs; do
            if [ -d "$themes_src/$theme" ]; then
                cp -r "$themes_src/$theme" "$themes_dst/"
            fi
        done

        log_info "Copied GTK themes"
    else
        log_warn "GTK themes directory not found: $themes_src"
    fi
}

# Funzione per creare il README.txt
create_readme() {
    log_info "Creating README.txt..."

    cat > "$DIST_DIR/README.txt" <<'EOF'
================================================================================
                        deskHPSDR for Windows
================================================================================

deskHPSDR is an SDR transceiver frontend for HPSDR protocol-compatible devices,
primarily designed for Hermes Lite 2.

SYSTEM REQUIREMENTS
-------------------
- Windows 7 or higher (64-bit)
- No additional software installation required
- All dependencies are included in this package

INSTALLATION
------------
1. Extract all contents of this archive to a folder of your choice
2. Double-click on deskhpsdr.exe to run the application

FIRST RUN
---------
- The first launch may take a few seconds to initialize GTK
- If your antivirus software blocks the executable, you may need to add an
  exception for deskhpsdr.exe

PACKAGE CONTENTS
----------------
This portable distribution includes:
- deskhpsdr.exe         : Main application executable
- *.dll                 : All required runtime libraries (GTK3, GLib, etc.)
- lib/                  : GDK-Pixbuf image loaders
- share/icons/          : Icon themes (Adwaita, hicolor)
- share/glib-2.0/       : GLib schemas
- share/themes/         : GTK themes

NOTES
-----
- This is a portable distribution - no installation is required
- You can run the application from any location (USB drive, desktop, etc.)
- Configuration files will be stored in your Windows user profile

RUNNING WITH WINE (Linux/macOS)
-------------------------------
If you want to test the application with Wine on Linux or macOS:

1. Install Wine (version 3.0 or higher recommended)
2. Run: wine deskhpsdr.exe

Known Wine warnings (these are non-critical and can be ignored):
- "Failed to load module gail:atk-bridge" - GTK accessibility module
- "ntlm_auth was not found" - Windows authentication (not needed)
- GDK backend warnings - display system compatibility

The application should work despite these warnings.

SUPPORT & DOCUMENTATION
-----------------------
For issues, documentation, and source code:
https://github.com/[your-repo]/deskhpsdr

HARDWARE COMPATIBILITY
----------------------
This Windows port supports:
- Network-based HPSDR devices (Hermes Lite 2, etc.)
- Audio I/O via PortAudio
- MIDI controllers

Not supported on Windows:
- GPIO hardware interfaces
- SoapySDR devices
- Direct USB hardware access

LICENSE
-------
See LICENSE file for details.

================================================================================
EOF

    log_info "README.txt created"
}

# Funzione principale
main() {
    log_info "========================================"
    log_info "  deskHPSDR Windows Packaging Script"
    log_info "========================================"
    echo

    # Verifica che l'eseguibile esista
    if [ ! -f "$BUILD_DIR/$EXE_NAME" ]; then
        log_error "Executable not found: $BUILD_DIR/$EXE_NAME"
        log_error "Please build the project first with: cmake --build build"
        exit 1
    fi

    log_info "Found executable: $BUILD_DIR/$EXE_NAME ($(du -h "$BUILD_DIR/$EXE_NAME" | cut -f1))"
    echo

    # Crea directory di distribuzione
    log_info "Creating distribution directory..."
    rm -rf "$DIST_DIR"
    mkdir -p "$DIST_DIR"

    # Copia l'eseguibile
    log_info "Copying executable..."
    cp "$BUILD_DIR/$EXE_NAME" "$DIST_DIR/"
    echo

    # Raccoglie le dipendenze DLL
    log_info "Analyzing DLL dependencies (this may take a moment)..."
    dll_list=$(mktemp)
    collect_dll_dependencies "$BUILD_DIR/$EXE_NAME" "$dll_list"

    # Rimuovi duplicati e ordina
    sort -u "$dll_list" -o "$dll_list"

    log_info "Found $(wc -l < "$dll_list") DLL dependencies"
    echo

    # Copia le DLL
    copy_dlls "$dll_list"
    rm "$dll_list"
    echo

    # Copia DLL opzionali ma utili (non dipendenze dirette, ma richieste da GTK a runtime)
    log_info "Copying optional GTK runtime DLLs..."
    optional_dlls="libgailutil-3-0.dll"
    for dll in $optional_dlls; do
        if [ -f "$MINGW_BIN/$dll" ]; then
            cp "$MINGW_BIN/$dll" "$DIST_DIR/"
            log_info "  + $dll"
        fi
    done
    echo

    # Copia i file di runtime
    copy_pixbuf_loaders
    echo

    copy_icons
    echo

    copy_glib_schemas
    echo

    copy_themes
    echo

    # Crea README
    create_readme
    echo

    # Calcola dimensioni
    dist_size=$(du -sh "$DIST_DIR" | cut -f1)
    log_info "Distribution directory size: $dist_size"
    echo

    # Crea archivio ZIP
    log_info "Creating ZIP archive..."
    cd "$(dirname "$DIST_DIR")"
    zip_name="deskhpsdr-windows-$(date +%Y%m%d).zip"

    # Rimuovi archivio esistente se presente
    rm -f "$zip_name"

    # Crea ZIP (silenzioso per non riempire lo schermo)
    zip -r -q "$zip_name" "$(basename "$DIST_DIR")"

    zip_size=$(du -sh "$zip_name" | cut -f1)
    log_info "Created: dist/$zip_name ($zip_size)"
    echo

    # Riepilogo
    log_info "========================================"
    log_info "  Packaging completed successfully!"
    log_info "========================================"
    echo
    log_info "Distribution package: $SCRIPT_DIR/dist/$zip_name"
    log_info "Uncompressed size: $dist_size"
    log_info "Compressed size: $zip_size"
    echo
    log_info "You can now copy this ZIP file to a Windows machine and extract it."
    log_info "No installation required - just extract and run deskhpsdr.exe!"
    echo
}

# Esegui main
main "$@"

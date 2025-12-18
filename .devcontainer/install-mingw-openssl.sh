#!/bin/bash
# Script to build and install OpenSSL for MinGW-w64 cross-compilation

set -e

OPENSSL_VERSION="3.0.12"
PREFIX="/usr/x86_64-w64-mingw32"
BUILD_DIR="/tmp/openssl-build"

echo "Building OpenSSL ${OPENSSL_VERSION} for MinGW-w64..."

# Create build directory
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

# Download OpenSSL
if [ ! -f "openssl-${OPENSSL_VERSION}.tar.gz" ]; then
    wget "https://www.openssl.org/source/openssl-${OPENSSL_VERSION}.tar.gz"
fi

# Extract
tar xzf "openssl-${OPENSSL_VERSION}.tar.gz"
cd "openssl-${OPENSSL_VERSION}"

# Configure for MinGW-w64
./Configure mingw64 \
    --cross-compile-prefix=x86_64-w64-mingw32- \
    --prefix="${PREFIX}" \
    --openssldir="${PREFIX}/ssl" \
    no-shared \
    no-asm

# Build
make -j$(nproc)

# Install
sudo make install_sw

# Create pkg-config file
sudo tee "${PREFIX}/lib/pkgconfig/openssl.pc" > /dev/null << EOF
prefix=${PREFIX}
exec_prefix=\${prefix}
libdir=\${exec_prefix}/lib
includedir=\${prefix}/include

Name: OpenSSL
Description: Secure Sockets Layer and cryptography libraries
Version: ${OPENSSL_VERSION}
Requires: libssl libcrypto
EOF

sudo tee "${PREFIX}/lib/pkgconfig/libssl.pc" > /dev/null << EOF
prefix=${PREFIX}
exec_prefix=\${prefix}
libdir=\${exec_prefix}/lib
includedir=\${prefix}/include

Name: OpenSSL-libssl
Description: Secure Sockets Layer and cryptography libraries
Version: ${OPENSSL_VERSION}
Requires.private: libcrypto
Libs: -L\${libdir} -lssl
Cflags: -I\${includedir}
EOF

sudo tee "${PREFIX}/lib/pkgconfig/libcrypto.pc" > /dev/null << EOF
prefix=${PREFIX}
exec_prefix=\${prefix}
libdir=\${exec_prefix}/lib
includedir=\${prefix}/include

Name: OpenSSL-libcrypto
Description: OpenSSL cryptography library
Version: ${OPENSSL_VERSION}
Libs: -L\${libdir} -lcrypto
Libs.private: -lws2_32 -lgdi32 -lcrypt32
Cflags: -I\${includedir}
EOF

# Cleanup
cd /
rm -rf "${BUILD_DIR}"

echo "OpenSSL ${OPENSSL_VERSION} installed successfully for MinGW-w64"

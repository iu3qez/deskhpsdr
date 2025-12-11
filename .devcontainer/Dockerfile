FROM ubuntu:22.04

# Evita prompt interattivi durante l'installazione
ENV DEBIAN_FRONTEND=noninteractive

# Installa MinGW cross-compiler e dipendenze
RUN apt-get update && apt-get install -y \
    # Build tools
    cmake \
    make \
    git \
    pkg-config \
    # MinGW cross-compiler
    mingw-w64 \
    g++-mingw-w64-x86-64 \
    gcc-mingw-w64-x86-64 \
    # Dipendenze MinGW per il progetto
    libz-mingw-w64-dev \
    # Utility
    wget \
    unzip \
    tar \
    && rm -rf /var/lib/apt/lists/*

# Crea directory per le librerie cross-compilate
WORKDIR /opt/mingw-deps

# Installa le dipendenze mancanti compilandole da source
# (GTK3, FFTW3, OpenSSL, libcurl, libxml2, json-c, portaudio per MinGW)

# FFTW3
RUN wget http://www.fftw.org/fftw-3.3.10.tar.gz && \
    tar xzf fftw-3.3.10.tar.gz && \
    cd fftw-3.3.10 && \
    ./configure --host=x86_64-w64-mingw32 --prefix=/usr/x86_64-w64-mingw32 \
                --enable-static --disable-shared --enable-threads --with-combined-threads && \
    make -j$(nproc) && make install && \
    cd .. && rm -rf fftw-3.3.10*

# OpenSSL
RUN wget https://www.openssl.org/source/openssl-3.0.12.tar.gz && \
    tar xzf openssl-3.0.12.tar.gz && \
    cd openssl-3.0.12 && \
    ./Configure mingw64 --cross-compile-prefix=x86_64-w64-mingw32- \
                --prefix=/usr/x86_64-w64-mingw32 no-shared && \
    make -j$(nproc) && make install_sw && \
    cd .. && rm -rf openssl-3.0.12*

# libxml2
RUN wget https://download.gnome.org/sources/libxml2/2.11/libxml2-2.11.5.tar.xz && \
    tar xf libxml2-2.11.5.tar.xz && \
    cd libxml2-2.11.5 && \
    ./configure --host=x86_64-w64-mingw32 --prefix=/usr/x86_64-w64-mingw32 \
                --enable-static --disable-shared --without-python && \
    make -j$(nproc) && make install && \
    cd .. && rm -rf libxml2-2.11.5*

# json-c
RUN wget https://s3.amazonaws.com/json-c_releases/releases/json-c-0.17.tar.gz && \
    tar xzf json-c-0.17.tar.gz && \
    cd json-c-0.17 && \
    mkdir build && cd build && \
    cmake -DCMAKE_TOOLCHAIN_FILE=/opt/mingw-toolchain.cmake \
          -DCMAKE_INSTALL_PREFIX=/usr/x86_64-w64-mingw32 \
          -DBUILD_SHARED_LIBS=OFF .. && \
    make -j$(nproc) && make install && \
    cd ../.. && rm -rf json-c-0.17*

# libcurl
RUN wget https://curl.se/download/curl-8.5.0.tar.gz && \
    tar xzf curl-8.5.0.tar.gz && \
    cd curl-8.5.0 && \
    ./configure --host=x86_64-w64-mingw32 --prefix=/usr/x86_64-w64-mingw32 \
                --enable-static --disable-shared --with-openssl \
                --without-libpsl --without-brotli && \
    make -j$(nproc) && make install && \
    cd .. && rm -rf curl-8.5.0*

# PortAudio
RUN wget http://files.portaudio.com/archives/pa_stable_v190700_20210406.tgz && \
    tar xzf pa_stable_v190700_20210406.tgz && \
    cd portaudio && \
    ./configure --host=x86_64-w64-mingw32 --prefix=/usr/x86_64-w64-mingw32 \
                --enable-static --disable-shared && \
    make -j$(nproc) && make install && \
    cd .. && rm -rf portaudio pa_stable_v190700_20210406.tgz

# GTK3 per MinGW è complesso, useremo binari pre-compilati
RUN wget https://github.com/tschoonj/GTK-for-Windows-Runtime-Environment-Installer/releases/download/2022-01-04/gtk3-runtime-3.24.31-2022-01-04-ts-win64.exe && \
    apt-get update && apt-get install -y p7zip-full && \
    7z x gtk3-runtime-3.24.31-2022-01-04-ts-win64.exe -o/opt/gtk3-mingw && \
    cp -r /opt/gtk3-mingw/bin/* /usr/x86_64-w64-mingw32/bin/ || true && \
    cp -r /opt/gtk3-mingw/include/* /usr/x86_64-w64-mingw32/include/ || true && \
    cp -r /opt/gtk3-mingw/lib/* /usr/x86_64-w64-mingw32/lib/ || true && \
    rm -rf /opt/gtk3-mingw gtk3-runtime-*.exe && \
    apt-get remove -y p7zip-full && apt-get autoremove -y && \
    rm -rf /var/lib/apt/lists/*

# Crea toolchain file per CMake
RUN echo 'set(CMAKE_SYSTEM_NAME Windows)' > /opt/mingw-toolchain.cmake && \
    echo 'set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)' >> /opt/mingw-toolchain.cmake && \
    echo 'set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)' >> /opt/mingw-toolchain.cmake && \
    echo 'set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)' >> /opt/mingw-toolchain.cmake && \
    echo 'set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32)' >> /opt/mingw-toolchain.cmake && \
    echo 'set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)' >> /opt/mingw-toolchain.cmake && \
    echo 'set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)' >> /opt/mingw-toolchain.cmake && \
    echo 'set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)' >> /opt/mingw-toolchain.cmake && \
    echo 'set(PKG_CONFIG_EXECUTABLE /usr/bin/x86_64-w64-mingw32-pkg-config)' >> /opt/mingw-toolchain.cmake

# Configura PKG_CONFIG per MinGW
ENV PKG_CONFIG_PATH=/usr/x86_64-w64-mingw32/lib/pkgconfig
ENV PKG_CONFIG_LIBDIR=/usr/x86_64-w64-mingw32/lib/pkgconfig

WORKDIR /workspace

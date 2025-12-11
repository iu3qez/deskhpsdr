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

# Crea toolchain file per CMake (necessario per json-c e altri)
RUN echo 'set(CMAKE_SYSTEM_NAME Windows)' > /opt/mingw-toolchain.cmake && \
    echo 'set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)' >> /opt/mingw-toolchain.cmake && \
    echo 'set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)' >> /opt/mingw-toolchain.cmake && \
    echo 'set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)' >> /opt/mingw-toolchain.cmake && \
    echo 'set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32)' >> /opt/mingw-toolchain.cmake && \
    echo 'set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)' >> /opt/mingw-toolchain.cmake && \
    echo 'set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)' >> /opt/mingw-toolchain.cmake && \
    echo 'set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)' >> /opt/mingw-toolchain.cmake && \
    echo 'set(PKG_CONFIG_EXECUTABLE /usr/bin/x86_64-w64-mingw32-pkg-config)' >> /opt/mingw-toolchain.cmake

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
    LDFLAGS="-L/usr/x86_64-w64-mingw32/lib" \
    CPPFLAGS="-I/usr/x86_64-w64-mingw32/include" \
    LIBS="-lws2_32 -lgdi32 -lcrypt32" \
    ./configure --host=x86_64-w64-mingw32 --prefix=/usr/x86_64-w64-mingw32 \
                --enable-static --disable-shared \
                --with-openssl=/usr/x86_64-w64-mingw32 \
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

# GTK3 e dipendenze per MinGW - usa pacchetti MSYS2 pre-compilati
RUN apt-get update && apt-get install -y zstd && \
    cd /opt/mingw-deps && \
    # Scarica pacchetti GTK3 e dipendenze da MSYS2 repo
    # Lista completa di dipendenze GTK3 per evitare errori incrementali
    PACKAGES="mingw-w64-x86_64-gtk3 \
              mingw-w64-x86_64-glib2 mingw-w64-x86_64-cairo mingw-w64-x86_64-pango \
              mingw-w64-x86_64-gdk-pixbuf2 mingw-w64-x86_64-atk mingw-w64-x86_64-harfbuzz \
              mingw-w64-x86_64-freetype mingw-w64-x86_64-fontconfig mingw-w64-x86_64-pixman \
              mingw-w64-x86_64-libpng mingw-w64-x86_64-libjpeg-turbo mingw-w64-x86_64-libtiff \
              mingw-w64-x86_64-libwebp mingw-w64-x86_64-libdeflate \
              mingw-w64-x86_64-gettext mingw-w64-x86_64-pcre2 mingw-w64-x86_64-libffi \
              mingw-w64-x86_64-expat mingw-w64-x86_64-bzip2 mingw-w64-x86_64-brotli \
              mingw-w64-x86_64-graphite2 mingw-w64-x86_64-fribidi mingw-w64-x86_64-libthai \
              mingw-w64-x86_64-libdatrie mingw-w64-x86_64-zlib mingw-w64-x86_64-libepoxy \
              mingw-w64-x86_64-hicolor-icon-theme mingw-w64-x86_64-adwaita-icon-theme \
              mingw-w64-x86_64-gdk-pixbuf2 mingw-w64-x86_64-librsvg mingw-w64-x86_64-libxml2 \
              mingw-w64-x86_64-shared-mime-info mingw-w64-x86_64-lzo2 mingw-w64-x86_64-lerc \
              mingw-w64-x86_64-xz mingw-w64-x86_64-jbigkit mingw-w64-x86_64-zstd" && \
    MSYS2_MIRROR="https://mirror.msys2.org/mingw/mingw64" && \
    # Funzione per scaricare ed estrarre pacchetto MSYS2
    for pkg_name in $PACKAGES; do \
        echo "Cercando $pkg_name..." && \
        # Ottieni l'ultima versione del pacchetto dal repo
        pkg_file=$(wget -qO- "${MSYS2_MIRROR}/" | grep -oP "${pkg_name}-[0-9][^\"]*\.pkg\.tar\.zst" | head -1) && \
        if [ -n "$pkg_file" ]; then \
            echo "Scarico $pkg_file..." && \
            wget -q "${MSYS2_MIRROR}/${pkg_file}" && \
            echo "Estraggo $pkg_file..." && \
            tar --zstd -xf "$pkg_file" -C /usr/x86_64-w64-mingw32 --strip-components=1 && \
            rm "$pkg_file"; \
        else \
            echo "ATTENZIONE: $pkg_name non trovato"; \
        fi; \
    done && \
    apt-get remove -y zstd && apt-get autoremove -y && \
    rm -rf /var/lib/apt/lists/*

# Configura PKG_CONFIG per MinGW
ENV PKG_CONFIG_PATH=/usr/x86_64-w64-mingw32/lib/pkgconfig:/usr/x86_64-w64-mingw32/lib64/pkgconfig
ENV PKG_CONFIG_LIBDIR=/usr/x86_64-w64-mingw32/lib/pkgconfig:/usr/x86_64-w64-mingw32/lib64/pkgconfig

WORKDIR /workspace

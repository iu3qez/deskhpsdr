# Claude Code - Windows Port Documentation

## Build Instructions

### DevContainer Setup
Il progetto utilizza un devcontainer configurato per cross-compilazione Windows usando MinGW-w64.

**Prerequisiti:**
- Docker
- VSCode con estensione Dev Containers

**Build tramite CMake:**
```bash
# Nel devcontainer
cd /workspaces/deskhpsdr

# Crea directory build
mkdir -p build
cd build

# Configura CMake con toolchain MinGW
cmake -DCMAKE_TOOLCHAIN_FILE=/opt/mingw-toolchain.cmake ..

# Compila (dalla directory build!)
make -j4
```

**IMPORTANTE**:
- Usare sempre `make` dalla directory `build/`, mai dalla root del progetto
- La root contiene un Makefile che non è compatibile con la build MinGW
- Il toolchain file si trova in `/opt/mingw-toolchain.cmake`
- I diagnostici dell'IDE sono falsi positivi in questo caso, perché l'IDE non è configurato per il toolchain MinGW cross-compilation

### Dipendenze
Tutte le dipendenze sono installate automaticamente nel devcontainer tramite il [Dockerfile](.devcontainer/Dockerfile):

**Compilate da source:**
- FFTW3
- OpenSSL
- libcurl
- libxml2
- json-c
- PortAudio

**Da MSYS2 (pacchetti pre-compilati):**
- GTK3 e tutte le sue dipendenze
- libiconv
- libusb

## Windows Compatibility Layer

### Principio Guida
**Ogni modifica al codice per supportare Windows deve, quando possibile, passare attraverso il layer di compatibilità.**

### File del Layer di Compatibilità
- **[src/windows_compat.h](src/windows_compat.h)** - Header con macro, typedef e dichiarazioni
- **[src/windows_compat.c](src/windows_compat.c)** - Implementazioni delle funzioni di compatibilità

### Esempi di Uso Corretto

#### ✅ CORRETTO - Wrapper nel layer di compatibilità
```c
// In windows_compat.h
#ifdef _WIN32
  #define gmtime_r(timep, result) (gmtime_s((result), (timep)) == 0 ? (result) : NULL)
#endif

// Nel codice sorgente
gmtime_r(&time, &result);  // Funziona sia su POSIX che Windows
```

#### ❌ SBAGLIATO - `#ifdef` inline nel codice
```c
// NON fare questo nel codice applicativo!
#ifdef _WIN32
  gmtime_s(&result, &time);
#else
  gmtime_r(&time, &result);
#endif
```

### Quando Usare il Layer di Compatibilità

1. **Funzioni POSIX non disponibili su Windows**
   - Esempio: `gmtime_r` → wrapper per `gmtime_s`
   - Esempio: `usleep` → macro per `Sleep`

2. **Strutture dati POSIX**
   - Esempio: `struct iovec`, `struct msghdr`
   - Definite completamente in `windows_compat.h`

3. **Include header system-specific**
   - Windows: `<winsock2.h>`, `<windows.h>`
   - POSIX: `<sys/socket.h>`, `<unistd.h>`
   - Centralizzati in `windows_compat.h`

4. **Costanti e macro**
   - Esempio: Baud rate constants (`B9600`, `B19200`)
   - Esempio: Socket options (`SO_REUSEPORT`)

### Quando NON Usare il Layer di Compatibilità

Quando il codice è intrinsecamente platform-specific e non può essere astratto:
- Funzioni che esistono solo su una piattaforma (es. `launch_serial_rigctl` per Linux)
- Usa `#ifndef _WIN32` / `#else` / `#endif` direttamente nel file sorgente
- Aggiungi un **FIXME** comment per indicare che serve implementazione

## Stub e Implementazioni Incomplete

### Documentazione con FIXME e TODO

Tutte le implementazioni incomplete o stub sono marcate con:
- **FIXME**: Indica codice che necessita di implementazione corretta
- **TODO**: Indica task o miglioramenti da fare

#### Esempio di Stub Documentato
```c
// FIXME: Implement proper sendmsg/recvmsg for Windows
// These are stubs for compilation - need proper scatter-gather I/O implementation
ssize_t sendmsg(SOCKET sockfd, const struct msghdr *msg, int flags) {
    // Simplified implementation: send first iovec buffer only
    if (msg && msg->msg_iov && msg->msg_iovlen > 0) {
        return sendto(sockfd, ...);
    }
    return -1;
}
```

### Lista Stub Correnti

Vedi [TODO-WINDOWS.md](TODO-WINDOWS.md) per la lista completa. Principali stub:

1. **`launch_serial_rigctl()` (src/rigctl.c:7428)**
   - Stub: Ritorna 0 su Windows
   - TODO: Implementare supporto porte seriali Windows

2. **`sendmsg()` / `recvmsg()` (src/windows_compat.c:185)**
   - Stub: Usa solo il primo buffer iovec
   - TODO: Implementare scatter-gather I/O completo con `WSASendMsg`/`WSARecvMsg`

## Workflow per Nuove Modifiche

### 1. Identificare il Problema
Quando si incontra un errore di compilazione per mancanza di funzione/header POSIX:

### 2. Valutare la Soluzione
**Domanda**: Questa funzionalità può essere astratta in modo cross-platform?

- **SÌ** → Aggiungi al layer di compatibilità
  - Wrapper/macro in `windows_compat.h`
  - Implementazione in `windows_compat.c` (se necessario)

- **NO** → Usa `#ifndef _WIN32` nel file sorgente
  - Wrappa il codice platform-specific
  - Aggiungi FIXME se serve implementazione Windows

### 3. Documentare
- Aggiungi commenti FIXME/TODO nel codice
- Aggiorna [TODO-WINDOWS.md](TODO-WINDOWS.md)
- Se è una modifica al layer di compatibilità, documenta in questo file

### 4. Testare
```bash
cd build
make clean
cmake -DCMAKE_TOOLCHAIN_FILE=/opt/mingw-toolchain.cmake ..
make -j4
```

## Pattern Comuni

### Include Headers Platform-Specific
```c
#ifndef _WIN32
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/time.h>
#include <semaphore.h>
#endif
#include "windows_compat.h"  // Sempre dopo gli include POSIX
```

### Funzioni Platform-Specific
```c
#ifndef _WIN32
// Implementazione POSIX
int do_something_posix() {
    // ... codice POSIX ...
}
#else
// FIXME: Implement do_something for Windows
int do_something_posix() {
    return -1;  // Stub
}
#endif
```

### Wrapper per Funzioni con Signature Diversa
```c
// In windows_compat.h
#ifdef _WIN32
  // Windows usa ordine parametri diverso per gmtime_s
  #define gmtime_r(timep, result) (gmtime_s((result), (timep)) == 0 ? (result) : NULL)
#endif
```

## Note Importanti

1. **Case Sensitivity**: Windows filesystem è case-insensitive ma MinGW cross-compiler su Linux è case-sensitive
   - Esempio: `Windows.h` vs `windows.h` → usa sempre minuscolo

2. **Path nei file .pc**: I pacchetti MSYS2 hanno path `/mingw64` che devono essere corretti
   - Fix automatico nel Dockerfile: `sed -i 's|/mingw64|/usr/x86_64-w64-mingw32|g'`

3. **PKG_CONFIG_PATH**: Deve includere sia `lib/pkgconfig` che `lib64/pkgconfig`
   - OpenSSL installa i suoi `.pc` in `lib64/pkgconfig`

4. **Non usare il Makefile root**: C'è un Makefile nella root che cerca WebKitGTK e altre dipendenze Linux
   - Sempre usare `make` dalla directory `build/`

## Riferimenti

- Dockerfile: [.devcontainer/Dockerfile](.devcontainer/Dockerfile)
- Windows Compat Layer: [src/windows_compat.h](src/windows_compat.h), [src/windows_compat.c](src/windows_compat.c)
- TODO List: [TODO-WINDOWS.md](TODO-WINDOWS.md)
- CMake Toolchain: `/opt/mingw-toolchain.cmake` (nel container)

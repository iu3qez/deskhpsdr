# piHPSDR Windows Porting - Guida e Regole

## Stato Attuale

Il codice nel repository è quello **originale Linux**, non portato. Un precedente tentativo di porting è stato abbandonato perché i branch si erano allontanati troppo e non era possibile riconciliarli.

Nella cartella `Windows/` esistono già:
- Layer di compatibilità funzionanti (`windows_compat.h`, `windows_compat.c`)
- Header wrapper per intercettare include POSIX
- MIDI implementation per Windows (`windows_midi.c`)
- Script di build e documentazione

---

## Regole di Portabilità (FONDAMENTALI)

### 1. Minimo Impatto sul Codice Originale
- **NON modificare** il codice sorgente originale se non strettamente necessario
- Preferire **sempre** i wrapper rispetto alle modifiche dirette
- Se servono `#ifdef _WIN32`, usarli ma **senza modificare la logica** del codice

### 2. Uso dei Wrapper
- Gli header wrapper in `Windows/` intercettano gli include POSIX
- Pattern: `#include <pthread.h>` → viene intercettato da `Windows/pthread.h` → include `windows_compat.h`
- Il codice sorgente **non deve sapere** di essere su Windows

### 3. Documentazione Obbligatoria
- **Ogni modifica** al codice originale deve essere documentata in questo file
- Specificare: file, riga, motivo, alternativa considerata

### 4. Esclusione Codice Non Necessario
- Alcune parti del codice sono "difficili" e **non servono su Windows**
- Queste vanno **escluse completamente** dalla compilazione Windows (non solo via config)
- Esempio: **GPIO** - specifico Raspberry Pi, da escludere del tutto

| Componente | File | Motivo Esclusione |
|------------|------|-------------------|
| GPIO | `gpio.c`, `i2c.c`, `andromeda.c` | Hardware Raspberry Pi (libgpiod, i2c-dev) |
| SATURN | `saturnmain.c`, `saturnserver.c`, `saturndrivers.c`, `saturnregisters.c`, `saturn_menu.c` | Driver kernel Linux PCIe |
| udplistener | `udplistener.c` | Usa fork(), execl(), posix_openpt() - non portabile |
| Simulatori | `hpsdrsim.c`, `bootldrsim.c` | Usano mmap(), signal handlers complessi |
| ALSA MIDI | `alsa_midi.c` | Specifico Linux, usare windows_midi.c |
| ALSA Audio | `audio.c` (quando AUDIO=ALSA) | Specifico Linux, usare PortAudio |
| PulseAudio | `pulseaudio.c` | Specifico Linux, usare PortAudio |

### 5. Stub Richiedono Approvazione
- **IMPORTANTE**: Eventuali stub (implementazioni vuote/parziali) devono essere **APPROVATI PREVENTIVAMENTE**
- Prima di creare uno stub, chiedere conferma con: cosa fa, perché serve, alternativa

### 6. Build System
- **CMake** per il porting Windows (cross-compile da Linux)
- Il Makefile originale rimane per Linux/macOS
- CMake semplifica la cross-compilazione con MinGW

---

## Architettura del Layer di Compatibilità

### Header Wrapper Pattern
```
Windows/
├── pthread.h          → #include "windows_compat.h"
├── semaphore.h        → #include "windows_compat.h"
├── unistd.h           → #include "windows_compat.h"
├── fcntl.h            → #include "windows_compat.h"
├── poll.h             → #include "windows_compat.h"
├── netdb.h            → #include "windows_compat.h"
├── termios.h          → #include "windows_compat.h"
├── pwd.h              → #include "windows_compat.h"
├── endian.h           → #include "windows_compat.h"
├── ifaddrs.h          → #include "windows_compat.h"
├── arpa/
│   └── inet.h         → #include "../windows_compat.h"
├── net/
│   ├── if.h           → #include "../windows_compat.h"
│   └── if_arp.h       → #include "../windows_compat.h"
├── netinet/
│   ├── in.h           → #include "../windows_compat.h"
│   ├── ip.h           → #include "../windows_compat.h"
│   └── tcp.h          → #include "../windows_compat.h"
└── sys/
    ├── socket.h       → #include "../windows_compat.h"
    ├── ioctl.h        → #include "../windows_compat.h"
    ├── mman.h         → #include "../windows_compat.h"
    ├── select.h       → #include "../windows_compat.h"
    └── utsname.h      → #include "../windows_compat.h"
```

### Mappature POSIX → Windows (in windows_compat.h/c)

| POSIX | Windows | Note |
|-------|---------|------|
| `pthread_t` | `HANDLE` | CreateThread |
| `pthread_mutex_t` | `CRITICAL_SECTION` | |
| `sem_t` | `struct { HANDLE }` | CreateSemaphore |
| `close(socket)` | `closesocket()` | Macro |
| `sleep(x)` | `Sleep(x*1000)` | Macro |
| `usleep(x)` | `Sleep(x/1000)` | Macro |
| `clock_gettime()` | Implementazione custom | |
| `getifaddrs()` | `GetAdaptersAddresses()` | |
| `realpath()` | `GetFullPathNameA()` | |
| `fcntl()` | `ioctlsocket()` | Per O_NONBLOCK |

---

## Features Supportate su Windows

- GTK3 GUI
- PortAudio (audio)
- Windows MIDI API (WinMM)
- Network protocols (P1/P2 HPSDR)
- Client/server
- USB OZY (libusb)
- SoapySDR (opzionale)
- STEMlab/RedPitaya
- WDSP DSP library

## Features NON Supportate su Windows

- GPIO (specifico Raspberry Pi)
- SATURN/G2 XDMA (driver kernel Linux)
- PulseAudio/ALSA (usare PortAudio)
- ALSA MIDI (usare WinMM)

---

## Dipendenze Windows (MSYS2/MinGW64)

```
mingw-w64-x86_64-gcc
mingw-w64-x86_64-gtk3
mingw-w64-x86_64-portaudio
mingw-w64-x86_64-fftw
mingw-w64-x86_64-openssl
mingw-w64-x86_64-libusb
mingw-w64-x86_64-curl
mingw-w64-x86_64-cmake
```

---

## Modifiche al Codice Originale

### Regola: Conflitti Macro/Definizioni Windows

Windows definisce già alcune macro che confliggono con quelle del progetto.

**Soluzione per selezioni utente**: Rinominare con prefisso `selection_`

| Originale | Rinominato | Motivo |
|-----------|------------|--------|
| `SNB` | `selection_SNB` | Conflitto con Windows headers |

### Conflitti WDSP - Chiarimento

WDSP è **originariamente Windows** → `linux_port.h` emula i tipi Windows su Linux.

| Macro | File | Su Windows | Azione |
|-------|------|------------|--------|
| `DWORD` | linux_port.h | Già in windows.h | linux_port.h NON incluso su Windows |
| `TRUE/FALSE` | linux_port.h | Già in windows.h | linux_port.h NON incluso su Windows |
| `HANDLE` | linux_port.h | Già in windows.h | linux_port.h NON incluso su Windows |
| `LONG` | linux_port.h | Già in windows.h | linux_port.h NON incluso su Windows |

**Nota**: `linux_port.h` è protetto con `#if defined(linux) || defined(__APPLE__)` quindi su Windows NON viene incluso. I tipi vengono da `<windows.h>` direttamente.

### REGISTRO MODIFICHE
Ogni modifica al codice originale deve essere registrata qui.

| File | Tipo | Descrizione | Data |
|------|------|-------------|------|
| wdsp/comm.h:36 | Case fix | `Windows.h` → `windows.h` (cross-compile Linux case-sensitive) | 2025-12-16 |
| src/actions.h:241-244 | Rename | `RELATIVE`→`MODE_RELATIVE`, `ABSOLUTE`→`MODE_ABSOLUTE`, `PRESSED`→`MODE_PRESSED`, `RELEASED`→`MODE_RELEASED` (conflitto Windows) | 2025-12-16 |
| 16 file src/*.c | Rename | Aggiornati riferimenti a ACTION_MODE enum | 2025-12-16 |
| src/client_server.h:26 | Include | Aggiunto `#include <endian.h>` per htobe64/be64toh | 2025-12-16 |
| Windows/sys/resource.h | Nuovo | Wrapper per getrlimit/setrlimit (stub) | 2025-12-16 |

---

## Analisi WDSP

**IMPORTANTE**: WDSP è originariamente una libreria **Windows** portata su Linux/macOS!

Questo significa:
- `linux_port.h/c` emula i tipi Windows (DWORD, HANDLE, etc.) su Linux
- Su Windows questi tipi **esistono già nativamente**
- I conflitti sono causati da `linux_port.h` che ridefinisce cose già presenti in Windows
- Su Windows NON serve `windows_port.h` - la libreria è nel suo ambiente nativo

### Stato Attuale
- `linux_port.h/c`: Emula Windows API su Linux/macOS (mapping inverso!)
- Su Windows: usare direttamente le API native, **escludere** linux_port.h
- 7 blocchi `#ifdef _WIN32` già presenti per audio real-time (AVRT)

### Cosa Serve per Windows
1. **NON includere** `linux_port.h` su Windows (già fatto con `#if defined(linux) || defined(__APPLE__)`)
2. Verificare che `comm.h` gestisca correttamente il caso Windows
3. I tipi DWORD, HANDLE, etc. vengono da `<windows.h>` direttamente

### File WDSP Critici
- `wdsp/comm.h` - Include management (riga 27-33: esclude linux_port.h su Windows)
- `wdsp/linux_port.h` - Solo per Linux/macOS, NON usare su Windows
- `wdsp/main.c` - Audio priority (già ha #ifdef _WIN32 per AVRT)
- `wdsp/wisdom.c` - Console allocation (già ha #ifdef _WIN32)

---

## File Sorgente per Categoria

### Categoria A: Network (Alta Priorità)
File che usano socket, richiedono Winsock2 wrapper:
- `main.c`, `new_protocol.c`, `old_protocol.c`
- `discovery.c`, `new_discovery.c`, `old_discovery.c`
- `rigctl.c`, `protocols.c`, `client_thread.c`
- `bootloader.c`

### Categoria B: Threading/Audio (Media Priorità)
File che usano pthread/semaphore:
- `portaudio.c` - Audio (usare questo su Windows)
- `iambic.c` - CW keyer
- `receiver.c`, `transmitter.c`
- `waterfall.c`, `rx_panadapter.c`, `tx_panadapter.c`

### Categoria C: Casi Speciali (Richiedono Attenzione)
- `startup.c` - usa `getpwuid()`, `getuid()` → implementare wrapper
- `ozyio.c` - usa `readlink("/proc/self/exe")` → usare `GetModuleFileName()`

### Categoria D: Da Escludere (vedi sezione 4)
GPIO, SATURN, simulatori, ALSA

---

## TODO Porting

- [x] Creare CMakeLists.txt per Windows (cross-compile) - FATTO
- [ ] Verificare wrapper in `Windows/` coprono tutti i casi
- [ ] Verificare WDSP compila su Windows (linux_port.h già escluso)
- [ ] Implementare wrapper per `getpwuid()`, `readlink()` se mancanti
- [ ] Testare compilazione cross-compile
- [ ] Documentare ogni modifica nel REGISTRO MODIFICHE

---

## Note Tecniche

### Cross-Compilazione
- Host: Linux (Ubuntu/Debian)
- Target: Windows x86_64
- Toolchain: MinGW-w64 (x86_64-w64-mingw32-gcc)
- CMake toolchain file necessario

### Configurazione Directory Windows
- Config: `%LOCALAPPDATA%/piHPSDR` (non Documents per evitare sync cloud)
- Eseguibile: directory corrente con DLL

### Problemi Noti Risolti in Passato
- `#include <Windows.h>` → `#include <windows.h>` (case-sensitive su Linux)
- `#include <mmeapi.h>` → `#include <mmsystem.h>` (MinGW compatibility)
- `OnLinkPrefixLength` non disponibile in MinGW GCC 10

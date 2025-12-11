# TODO - Windows Port

## Problemi risolti con FIXME/stub

### [src/rigctl.c:7304](src/rigctl.c#L7304)
- **Problema**: Funzione `launch_serial_rigctl()` non implementata per Windows
- **Fix temporaneo**: Stub che ritorna 0 con `#else`
- **TODO**: Implementare supporto porte seriali per Windows usando API Windows (CreateFile, etc.)

### [src/windows_compat.h:126](src/windows_compat.h#L126) + [src/windows_compat.c:183](src/windows_compat.c#L183)
- **Problema**: Funzioni `sendmsg()`/`recvmsg()` non disponibili su Windows
- **Fix temporaneo**: Implementazione semplificata che usa solo il primo buffer iovec
- **TODO**: Implementare scatter-gather I/O completo usando `WSASendMsg`/`WSARecvMsg`

## File con dipendenze POSIX da verificare

### File che includono header POSIX
I seguenti file potrebbero avere problemi con header POSIX non disponibili su Windows:
- `soapy_discovery.c` - Include `SoapySDR/Device.h`
- Altri file sorgente da verificare per uso di:
  - `termios.h`
  - `unistd.h`
  - `fcntl.h`
  - `sys/time.h`
  - `semaphore.h`

**Azione richiesta**:
1. Cercare tutti i file che includono questi header
2. Wrappare con `#ifndef _WIN32` / `#endif`
3. Verificare che il codice che usa queste API sia anche wrappato

## Dipendenze mancanti nel Dockerfile

### Pacchetti MSYS2 aggiunti
- ✅ `mingw-w64-x86_64-libiconv` (per libxml2)
- ✅ `mingw-w64-x86_64-libusb` (per USB support)
- ✅ Fix path `/mingw64` → `/usr/x86_64-w64-mingw32` nei file `.pc`

### Da valutare
- SoapySDR per Windows (se necessario)
- pthread-win32 o equivalente (pthread è già incluso in MinGW?)

## Correzioni codice

### File modificati
- ✅ `wdsp-1.28/comm.h` - Fix `Windows.h` → `windows.h` (case sensitivity)
- ✅ `src/rigctl.c` - Aggiunto `#endif` e stub per Windows
- ✅ `src/saturnserver.c` - Wrappato include POSIX con `#ifndef _WIN32`
- ✅ `src/windows_compat.h` - Aggiunti:
  - Wrapper `gmtime_r` per `gmtime_s`
  - Strutture `iovec` e `msghdr`
  - Dichiarazioni `sendmsg`/`recvmsg`
- ✅ `src/windows_compat.c` - Implementazioni stub `sendmsg`/`recvmsg`

## Stato compilazione

### Librerie compilate con successo
- ✅ wdsp (libwdsp.a)
- ✅ libsolar (libsolar.a)
- ✅ libtelnet (libtelnet.a)

### File sorgente principali
- ⚠️ In corso - molti file compilano, alcuni con warning deprecation GTK3
- ❌ `soapy_discovery.c` - Manca SoapySDR header

## Prossimi passi

1. Risolvere il problema di SoapySDR (wrappare con `#ifndef _WIN32` se non necessario)
2. Completare la compilazione di tutti i file sorgente
3. Linkare l'eseguibile finale
4. Testare l'eseguibile su Windows
5. Implementare le funzionalità stub (serial port, sendmsg/recvmsg)

---
title: Le affermazioni di assenza in deskhpsdr vanno verificate per concetto, non per nome
date: 2026-08-30
category: codebase-navigation
module: src
problem_type: best_practice
component: codebase_structure
severity: medium
applies_when:
  - "Si sta per affermare che deskhpsdr non ha una feature, una proprietà o una configurazione"
  - "Si cerca una funzionalità nota da piHPSDR upstream dentro deskhpsdr"
  - "Si indaga il sottosistema audio macOS"
tags: [ricerca-codice, coreaudio, seriale, piHPSDR, fork]
---

# Le affermazioni di assenza in deskhpsdr vanno verificate per concetto, non per nome

## Context

Durante l'analisi del keying CW sono state fatte **due affermazioni di assenza rivelatesi false**, entrambe per lo stesso motivo: la ricerca era ancorata a un nome (di file o di funzione) invece che al concetto. Una delle due è finita in un documento pubblicato prima di essere corretta.

deskhpsdr è un fork di piHPSDR con rinomine e riorganizzazioni non documentate. La ricerca per nome ha quindi un tasso di falsi negativi strutturalmente alto.

## Guidance

**Prima di scrivere «deskhpsdr non ha X», cercare X per concetto:** la costante di sistema, l'ioctl, la chiamata di API, il nome del registro. Non la funzione che lo avvolge.

### Istanza 1 — il sottosistema audio macOS è spaccato in due file

I nomi ingannano: `macos_audio.c` sembra contenere tutto il codice audio macOS, e invece contiene solo la metà dei buffer.

| File | Contenuto |
|---|---|
| `src/macos_audio.c` | Ring buffer (`local_audio_buffer`, `sidetone_buffer`), render callback, `cw_audio_write()`, costanti `CW_LAT_LOW`/`TARGET`/`HIGH` = 128/256/384 sample |
| `src/coreaudio.c` | Configurazione AudioUnit e device: `coreaudio_tune_buffer_frames()`, che imposta `kAudioDevicePropertyBufferFrameSize` |

Errore commesso: cercato `kAudioDevicePropertyBufferFrameSize` solo in `macos_audio.c`, non trovato, dichiarato che il buffer resta al default di sistema (~512 frame, 10,7 ms).

Falso. I target sono in `coreaudio.c:39-41`:

```c
#define COREAUDIO_OUTPUT_BUFFER_TARGET 128        // 2,67 ms @ 48 kHz
#define COREAUDIO_INPUT_BUFFER_TARGET 256
#define COREAUDIO_TCI_MONITOR_BUFFER_TARGET 128
```

Conseguenza dell'errore: la stima di latenza del sidetone locale è passata da «16-25 ms, al limite dell'usabile» a **9-11 ms reali**, cioè meglio del percorso via radio. Un giudizio operativo ribaltato da una ricerca incompleta.

### Istanza 2 — il PTT su seriale ha un nome diverso da upstream

| | piHPSDR (dl1ycf) | deskhpsdr |
|---|---|---|
| Funzione | `launch_serial_ptt()` / `ptt_server()` | `launch_serptt()` / `monitor_serptt_cts_thread()` |
| Simboli | — | `serptt_fd`, `serptt_cts`, `stop_serptt()` |

Errore commesso: cercato il nome di upstream, non trovato, dichiarato assente.

deskhpsdr ha inoltre un **secondo** uso delle linee di modem che upstream non ha, `monitor_sertune_thread()` (`rigctl.c:350`): RTS = hold ATU su TUNE, DTR = stato TX, CTS = TX inhibit attivo basso.

Gli slot porta sono cinque, `SerialPorts[MAX_SERIAL + 2]` con `MAX_SERIAL 3` (`rigctl.h:41-42`): 0-2 rigctl/CAT, 3 sertune, 4 serPTT.

La ricerca per concetto — `TIOCM_CTS`, `TIOCM_RTS`, `TIOCM_DTR` — trova entrambi i sottosistemi al primo colpo.

## Why This Matters

Un'affermazione di assenza è asimmetrica: una presenza si prova con un `file:line`, un'assenza si prova solo con l'esaustività della ricerca. In un fork con rinomine non documentate l'esaustività per nome non esiste.

Il costo non è teorico. In questa sessione una delle due ha prodotto un numero sbagliato di un fattore due in un documento già archiviato nel repo, e ha portato a raccomandare complessità aggiuntiva (un sidetone nel microcontrollore) per recuperare una latenza che in gran parte non c'era.

## When to Apply

- Prima di scrivere che deskhpsdr «non supporta», «non imposta», «non ha» qualcosa
- Quando si porta una feature da piHPSDR e si vuole sapere se esiste già qui
- Quando una ricerca su un file «ovvio» torna vuota: è più probabile che il codice sia altrove che assente

## Examples

Sbagliato:

```sh
grep -n "kAudioDevicePropertyBufferFrameSize" src/macos_audio.c   # vuoto → conclusione errata
grep -rn "launch_serial_ptt" src/                                 # vuoto → conclusione errata
```

Giusto:

```sh
grep -rn "BufferFrameSize\|MaximumFramesPerSlice" src/            # trova coreaudio.c
grep -rn "TIOCM_CTS\|TIOCM_RTS\|TIOCM_DTR" src/                   # trova serptt E sertune
```

Regola generale: `grep -rn` su tutto `src/` con il **simbolo di sistema**, mai su un singolo file con il nome di un wrapper.

## Related

- `ce_docs/explainers/cw-keyer.html` — cablaggio del keyer CW, latenze dei tre percorsi di sidetone

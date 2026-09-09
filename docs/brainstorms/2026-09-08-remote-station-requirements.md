# Stazione remota su deskHPSDR — Documento dei requisiti

Versione 0.1 — 8 settembre 2026 — esito del brainstorming, da rivedere prima dell'implementazione.

Riferimenti di codice: `dl1bz/deskhpsdr` @ 2026-09-07, `n9bc/thetis-on-the-web` (`totw.html`, ~454 kB), spec TCI `ExpertSDR3/TCI`.

## SCOPE DI QUESTO PIANO (aggiunto 2026-09-08)

Questo worktree è il fork di deskHPSDR. Il piano copre SOLO la parte server:
SRV-01, SRV-02, SRV-03, SRV-04, SRV-05, SRV-06, SRV-07, SRV-10.
Esclusi: SRV-08 (DEC-01 = A in fase 1), tutto CLI-*, CW-*, N1M-*, AUD-* (altri repo / configurazione).
DEC-02 (formato frame type=4) va congelato come primo task del piano, leggendo il codice.
Precedente nel codice: l'estensione RTTY nativa in src/tci.c è già per-connessione e opt-in (rtty_enabled), pattern da replicare per SRV-01.

---

## 1. Obiettivo e perimetro

Operare da remoto, anche su WAN (4G/5G, link con perdita), una stazione HPSDR (HL2 / ANAN) il cui host è un PC Linux con **deskHPSDR**, senza toccare il core di deskHPSDR oltre a estensioni opzionali e retrocompatibili, e con un'interfaccia operatore **solo browser**.

In perimetro: controllo radio, spettro/waterfall, audio RX/TX, CW, integrazione N1MM+, pannello fisico via console MIDI.
Fuori perimetro: Zeus, ThetisLink, TCI Remote/Compactor (Windows, chiuso), NereusSDR (CW TX non implementato, sviluppo fermo), remoting del protocollo HPSDR su VPN L2.
Fallback dichiarato: Sunshine/Moonlight (desktop remoto, UDP, Opus+FEC) se i requisiti audio non fossero raggiunti in tempo.

## 2. Architettura

Principio: **un trasporto per ogni tipo di dato**, nessuno obbligato a fare ciò per cui non è nato.

| Piano | Trasporto | Produttore | Consumatore |
|---|---|---|---|
| Controllo radio | TCI (WebSocket/TCP, porta 40001) | deskHPSDR | TOTW |
| Spettro | TCI, frame binario a bin (estensione) | deskHPSDR | TOTW |
| Audio RX/TX | vedi DEC-01 (Mumble/UDP oppure Opus in TCI) | deskHPSDR → PipeWire | client |
| CW | tool CW remoto proprio → MIDI virtuale locale (`snd-virmidi`) | tool CW | deskHPSDR |
| Logger | CAT TS-2000 su TCP (`rigctl`) | deskHPSDR | N1MM+ |
| Pannello fisico | Web MIDI (browser) → TCI | console DJ | TOTW |

Tutto passa in un tunnel WireGuard: TCI e CAT non hanno autenticazione.

Due macchine, ruoli distinti:

| | Host radio (Linux) | Postazione operatore (Windows/altro) |
|---|---|---|
| Software | deskHPSDR, `snd-virmidi`, metà host del tool CW, murmur (se DEC-01 = A), WireGuard | browser + TOTW, N1MM+, client Mumble, metà client del tool CW (paddle/keyer dell'operatore), WireGuard |
| Vincoli OS | deskHPSDR: solo Linux/macOS | N1MM+: solo Windows — irrilevante per l'host, N1MM non vi gira mai |

## 3. Stato di partenza verificato

- deskHPSDR TCI server (libwebsockets): 58 comandi, si presenta come `ExpertSDR3,2.0 / SunSDR2QRP`; audio RX/TX float32 stereo 48 k (opzione 24 k), stream IQ al sample rate del SDR (48–384 k, nessun resampling, **un solo owner**), spot, `cw_macros`, RTTY nativo, lock TX/TUNE fra client, "set lock" per-parametro di 200 ms fra client. Header binario spec (64 byte: receiver, sample_rate, format, codec, crc, length, type, channels, reserv[8]).
- deskHPSDR **non** ha: spettro a bin verso l'esterno, export N1MM, attenuatore/banda via TCI, ingresso key seriale (solo CTS→PTT a 50 ms), client/server proprio.
- deskHPSDR CW: keyer firmware radio, keyer iambico software con ingresso MIDI, `KY` via CAT, `cw_macros` via TCI; manipolazione sagomata host-side in `transmitter.c` (`cw_ramp_rf`).
- deskHPSDR MIDI Linux: ALSA **rawmidi** (`hw:x,y`), thread dedicato con `poll()`; azioni key CW processate fuori dalla coda idle GTK.
- TOTW: single-file, vanilla JS, TCI 2.0; disegno alimentato da un solo array `IQ.fftResult[4096]` + `S.iqSR`/`S.iqCentre`; scheduler audio naive (`rxNextTime = now + 0.02` su ritardo); riconoscimento frame euristico tarato su Thetis.
- N1MM+: nessun tipo radio TCI (ExpertSDR = emulazione Kenwood); CAT su COM **o TCP**; CW mai via CAT (`KY` rifiutato per policy); spettro da UDP XML 13064 o altre sorgenti.

## 4. Requisiti

Priorità: **M** must, **S** should, **C** could. Stato: E = esiste, MOD = modifica, NEW = da scrivere.

### 4.1 deskHPSDR — server (fork personale, estensioni opt-in per-client)

| ID | Requisito | Pri | Stato |
|---|---|---|---|
| SRV-01 | Nessuna modifica al comportamento di default del TCI server: ogni estensione è attiva solo per il client che la negozia. | M | — |
| SRV-02 | Frame spettro `type=4`: sorgente `rx->pixel_samples` dopo `GetPixels()` (`receiver.c`), nessuna FFT aggiuntiva. Payload: `low_hz`, `high_hz`, `floor_db`, `scale_db` (float), `bins` uint8 (dB relativi, passo 0,5 dB). Header TCI invariato. | M | NEW |
| SRV-03 | Negoziazione `spectrum_start:<rx>,<bins>,<fps>;` / `spectrum_stop:<rx>;`. Default 512 bin, 10 fps. Decimazione **max-of-N** (mai media). | M | NEW |
| SRV-04 | fps adattivo: se il backlog di scrittura del WebSocket cresce, ridurre 20→10→5 fps; i frame spettro in ritardo si scartano, non si accodano. | S | NEW |
| SRV-05 | `spectrum_span:<rx>,<low>,<high>;` dal client: i bin coprono lo span visualizzato (un solo array per span). | S | NEW |
| SRV-06 | `permessage-deflate` abilitato in libwebsockets (compressione gratuita dei bin quantizzati, nessun codice client). | S | MOD |
| SRV-07 | Estensioni CAT-like via TCI, solo due: `rx_att_ex:<rx>,<dB>` (attenuatore a step) e `band_ex:<rx>,<band>` (cambio banda tramite band-stack, non `vfo:` secco). | S | NEW |
| SRV-08 | Audio Opus in TCI (solo se DEC-01 = B): `audio_stream_codec:opus;` per-client, header `codec=1`, ri-chunking 512→480/960 frame, mono RX, `OPUS_APPLICATION_AUDIO`, FEC in-band on. Client che non negoziano restano float32. | C | NEW |
| SRV-09 | Nessun export spettro N1MM (lo spettro è già in TOTW). | — | scartato |
| SRV-10 | Porta TCI 40001, rigctl TCP TS-2000 attivo, nessun ascolto fuori dall'interfaccia WireGuard. | M | E (config) |

### 4.2 TOTW — client browser (fork)

| ID | Requisito | Pri | Stato |
|---|---|---|---|
| CLI-01 | Compatibilità deskHPSDR: dispatch dei frame binari **solo** su `type` a offset 24; IQ riconosciuto anche a `sample_rate == 48000` (oggi `> 48000`, riga ~4878); audio con header spec 64 byte e `type=1` (oggi assunto header Thetis 8 byte → 7 frame di rumore ogni 512 campioni). | M | MOD |
| CLI-02 | `iq_samplerate` e porta TCI configurabili (oggi 192000 e 50001 cablati). | M | MOD |
| CLI-03 | Handler `type=4` → dequantizza, interpola K bin su `IQ.fftResult[4096]`, imposta `S.iqSR = high−low`, `S.iqCentre`, `IQ.fftReady`; `IQ.smooth = 0` in modalità bin (WDSP ha già mediato). Nessuna modifica al codice di disegno. | M | MOD |
| CLI-04 | Invio `spectrum_span` al cambio zoom; delta frame opzionale (`+=` sull'array persistente). | S | MOD |
| CLI-05 | Modalità spettro selezionabile: bin (default su WAN) / IQ+FFT (LAN). | S | MOD |
| CLI-06 | Web MIDI: `requestMIDIAccess`, dispatcher `onmidimessage` → tabella azioni, **learn mode**, persistenza `localStorage`, encoder relativi (two's complement / sign-magnitude) e assoluti 7 bit, coalescenza 30–50 ms (vince l'ultimo valore), feedback LED via MIDI out. Chromium/Firefox; Safari/iOS esclusi. | S | NEW |
| CLI-07 | Tabella azioni MIDI limitata ai comandi TCI stock + `rx_att_ex`/`band_ex`: VFO (jog), volume, drive, sql, RIT, filtro, modo, banda, ATT, PTT, tune, mute, split, NR/NB toggle. **CW escluso**. | S | NEW |
| CLI-08 | Se DEC-01 = B: decodifica Opus (WebCodecs `AudioDecoder`, fallback libopus WASM embedded), codifica mic Opus (`AudioEncoder`), **scheduler audio riscritto**: jitter buffer adattivo 60–100 ms, compensazione drift, PLC su deadline (`decode(NULL)`), scarto frame stantii dopo raffica, 2–3 frame di anticipo per `TX_CHRONO`. | C | MOD/NEW |
| CLI-09 | Servito via https (mic, Web MIDI, secure context) dietro WireGuard. | M | config |

### 4.3 Audio

| ID | Requisito | Pri |
|---|---|---|
| AUD-01 | Latenza end-to-end mouth-to-ear obiettivo ≤ 150 ms su WAN; comportamento deterministico sotto perdita 1–2 %. | M |
| AUD-02 | Banda audio ≤ 100 kbit/s per verso (Opus 32–64 kbit/s). Il PCM float32 TCI (3 Mbit/s) è escluso su WAN: con RTT 60 ms e 1 % di perdita TCP non regge ~2,4 Mbit/s. | M |
| AUD-03 | Recovery da perdita: PLC reale (Opus) o FEC; mai stallo con raffica successiva senza scarto. | M |
| AUD-04 | Opzione A — Mumble: `murmur` sull'host, client Mumble headless attaccato a null-sink/null-source PipeWire di deskHPSDR (`AUDIO=PULSE`), `local_audio_mute` per il monitor locale, TX mic nel sink usato da deskHPSDR come ingresso. Zero codice. | (DEC-01) |
| AUD-05 | Opzione B — Opus in TCI: SRV-08 + CLI-08. Un solo trasporto, resta TCP (recovery = PLC + scarto, non FEC). | (DEC-01) |

### 4.4 CW

| ID | Requisito | Pri | Stato |
|---|---|---|---|
| CW-01 | Il CW arriva da un tool proprio di CW remoto. Semantica minima: **eventi key-on / key-off** già temporizzati dall'operatore, più PTT opzionale. Nessun protocollo keyer (no WinKey, no testo), nessuna seriale. | M | tool esterno |
| CW-02 | Iniezione via MIDI virtuale locale: `modprobe snd-virmidi midi_devs=1` → `hw:Virtual,0` (rawmidi, visto da deskHPSDR) + client seq "Virtual Raw MIDI 1-0" (scritto dal tool). macOS: sorgente virtuale CoreMIDI. | M | config |
| CW-03 | Mapping: note-on/off su `CW_KEYER_KEYDOWN` + `CW_KEYER_PTT` (il tool possiede PTT e hang; `MIDI_cw_is_active` disabilita "CW handled in radio"). Alternativa: `CW_STRAIGHT_KEY` se PTT/break-in restano a deskHPSDR. Mai `CW_LEFT/RIGHT` (sono contatti paddle). | M | config |
| CW-04 | Sicurezza: il tool invia periodicamente lo *stato* (idempotente) e forza key-up + PTT-off su perdita di connessione; cap hardware di deskHPSDR (`cw_key_down = 960000` ≈ 20 s) resta come ultima rete. | M | tool |
| CW-05 | Il CW MIDI non passa per TCI: nessun conflitto con il lock `trx` dei client TCI; verificare sul banco la coesistenza PTT MIDI / `trx` TOTW. | M | test |
| CW-06 | Latenza di manipolazione = catena TX host→radio (buffer P1); accettata per break-in, non garantito QSK pieno. Eventuale evoluzione: key nel gateware (bit P1) — fuori perimetro. | — | nota |
| CW-07 | `KY` (CAT) e `cw_macros` (TCI) disponibili per logger non-N1MM e macro in TOTW. | C | E |

### 4.5 N1MM+

| ID | Requisito | Pri | Stato |
|---|---|---|---|
| N1M-01 | CAT: radio "TS-2000", porta `host:porta` TCP del `rigctl` di deskHPSDR attraverso il tunnel. Nessun com0com. | M | E |
| N1M-02 | CW da N1MM (macro F1–F12): **fuori perimetro per ora**. N1MM non usa `KY` e il tool CW non espone un keyer; N1MM serve per log e CAT, il CW lo manipola l'operatore. Riapertura quando/se il tool avrà un'emulazione keyer. | — | rinviato |
| N1M-03 | Spettro: nessuno (bandmap senza spettro; lo spettro è in TOTW). | — | scartato |
| N1M-04 | Audio N1MM (voice keyer, digitali): sul piano audio scelto in DEC-01. | S | — |

### 4.6 Rete e budget di banda (per verso, stima)

| Flusso | Configurazione | Banda |
|---|---|---|
| TCI controllo | — | trascurabile |
| Spettro bin | 512 bin × 8 bit × 10 fps | ~40 kbit/s (≈10 con deflate) |
| Audio | Opus 48 k mono | 32–64 kbit/s |
| CAT TS-2000 | polling N1MM | trascurabile |
| CW | eventi MIDI | trascurabile |
| **Totale** | | **< 200 kbit/s** |
| (escluso) IQ TCI 48 k float32 | | 3 Mbit/s |

Porte nel tunnel: 40001 (TCI), rigctl TCP, 443 (TOTW), 64738 (Mumble, se A).

## 5. Decisioni aperte

| ID | Decisione | Opzioni | Orientamento |
|---|---|---|---|
| DEC-01 | Piano audio | A) Mumble via PipeWire (zero codice, UDP, FEC) — B) Opus in TCI (SRV-08 + CLI-08, un solo trasporto, TCP) | Fase 1 = A per andare in aria subito; B come evoluzione se si vuole convergere su TCI. Non escludenti. |
| DEC-02 | Formato definitivo frame `type=4` | proposta in SRV-02 | congelare prima di scrivere server e client |
| DEC-03 | PTT del CW: tool (`CW_KEYER_PTT`) o deskHPSDR (`CW_STRAIGHT_KEY`) | — | tool, per coerenza con CW-04 |
| DEC-04 | Interfaccia N1MM ↔ tool CW | — | **rinviata**: il tool resta on/off + PTT, niente emulazione keyer in questa fase |

## 6. Piano di verifica

1. **TOTW stock vs deskHPSDR** (prima di ogni modifica): sequenza `audio_samplerate`/`audio_start`, frame `type=0/1`, click periodico a ~94 Hz atteso → conferma CLI-01.
2. CLI-01/02 applicati: audio pulito, IQ riconosciuto a 48 k.
3. SRV-02/03 + CLI-03: spettro a bin sovrapponibile all'IQ+FFT su LAN; banda misurata con `iftop`/`ss -i`.
4. WAN simulata con `tc netem` (RTT 60 ms, loss 1–2 %, jitter 20 ms): spettro fluido, audio secondo AUD-01/03, nessun accumulo di latenza in 30 min.
5. CW: virmidi → deskHPSDR, sidetone e RF; misura latenza key→RF con seconda RX; test di caduta connessione (CW-04).
6. N1MM: CAT TS-2000 su TCP nel tunnel, polling stabile, split/PTT.
7. MIDI: console DJ, learn mode, VFO da jog, LED di stato.

## 7. Note di governance

- Le estensioni deskHPSDR vivono in un fork personale; DL1BZ non accetta "playground" nel core. Retrocompatibilità totale (SRV-01) è il prerequisito per proporle upstream.
- TOTW è vanilla JS single-file: mantenere questa proprietà (WASM eventuale embedded in base64).
- Nessuna dipendenza da software Windows o chiuso nella catena.

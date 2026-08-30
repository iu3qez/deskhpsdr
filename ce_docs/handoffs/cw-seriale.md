---
artifact_contract: "ce-handoff/v1"
created_at: "2026-08-30T21:21:37Z"
title: "CW su porta seriale in deskhpsdr — brainstorming interrotto a Phase 2"
summary: "Vincoli di piattaforma verificati e tre approcci sul tavolo per leggere un paddle CW da porta seriale; nessuno scelto, e una quarta opzione via USB-MIDI renderebbe la feature non necessaria per l'utente."
keywords: ["deskhpsdr", "cw-keyer", "porta-seriale", "iambic", "coreaudio", "tiocmiwait", "usb-midi", "esp32"]
cwd: "/Users/sf/Developer/deskhpsdr"
resume_focus: "Decidere se riprendere dalla scelta A/B/C o dalla domanda a monte se la feature serva ancora"
repository: "iu3qez/deskhpsdr"
repo_root_sha: "5ecf980078506aa9eb523b8b8479627a0843088a"
branch: "master"
head: "9f303625158255b3ad42dfbf13ee0f15a7efccc7"
---

# CW su porta seriale in deskhpsdr

## Obiettivo e stato

L'utente (IU3QEZ) vuole poter manipolare in CW con un paddle collegato al PC, avendo il rig fisicamente in un'altra stanza. Il brainstorming `ce-brainstorm` si è fermato a **Phase 2 senza scegliere l'approccio**. Nessun piano in `ce_docs/plans/`. **Nessun file `.c` toccato** — l'unico lavoro nel tree è documentazione e configurazione.

Un'opzione emersa tardi renderebbe l'intera feature non necessaria *per l'utente*, e la domanda a monte è rimasta aperta.

## Decisioni dell'utente — non ri-litigare

Queste sono scelte dell'utente prese vedendo le alternative, non inferenze dell'agente:

- **Il keyer deve essere iambico.** Sue parole: *"Per essere usabile DEVE essere iambico."* Il tasto verticale è un caso minore che costa una linea di ingresso invece di due.
- **Fork ora, upstream dopo**, con l'astrazione progettata per macOS e Linux fin dall'inizio; il ramo Linux resta un buco dichiarato. Scelto contro "solo fork macOS" e "upstream subito".
- **Nessuna PR verso `dl1bz/deskhpsdr` prima che il percorso Linux sia pronto.** Salvato anche in memoria di progetto: `~/.claude/projects/-Users-sf-Developer-deskhpsdr/memory/no-upstream-pr-before-linux.md` *(machine-local)*.
- **Perché la seriale e non il MIDI:** un adattatore USB-CDC più due resistenze costa nulla ed è ciò che l'ecosistema ham ha già; un'interfaccia MIDI per paddle richiede un microcontrollore.

## Vincoli verificati nei sorgenti

Tutti controllati in sessione, non assunti:

- **macOS non ha `TIOCMIWAIT`.** L'SDK espone solo `TIOCMBIC TIOCMBIS TIOCMGDTRWAIT TIOCMGET TIOCMODG TIOCMODS TIOCMSDTRWAIT TIOCMSET`. Nessuna attesa bloccante sul cambio di una linea di modem: **polling obbligatorio**. Su Linux `TIOCMIWAIT` esiste ed è nettamente migliore — da cui la decisione sulle PR.
- **Il MIDI non fa polling.** `src/mac_midi.c:274` registra una callback CoreMIDI; il commento a riga 64 dice *"The OS takes care of everything"*. USB-MIDI è una classe di device nota al SO; le linee di modem sono livelli elettrici che nessuno strato notifica.
- **Driver kernel fuori discussione.** DriverKit richiede entitlement gestiti da Apple (`com.apple.developer.driverkit` + `.family.serial`), app bundle e `SystemExtensions`. deskHPSDR si compila con un Makefile. Vincolo di distribuzione, non tecnico.
- **Nessun timer periodico veloce esistente.** Il `g_timeout_add` più rapido è 20 ms. L'unico contesto ricorrente sotto i 10 ms è l'arrivo dei pacchetti: `process_control_bytes()` in `src/old_protocol.c:1850`, granularità reale ~2,6 ms a 48 kHz. La render callback CoreAudio è l'unico contesto real-time ma non può ospitare una `ioctl`.
- **Le linee di modem sono già occupate.** `monitor_sertune_thread()` in `src/rigctl.c:350` usa RTS = hold ATU su TUNE, DTR = stato TX, CTS = TX inhibit attivo basso. Il serPTT usa un altro slot. Gli slot sono cinque, `SerialPorts[MAX_SERIAL + 2]` con `MAX_SERIAL 3` (`src/rigctl.h:41-42`): 0-2 rigctl/CAT, 3 sertune, 4 serPTT. **Una porta per il tasto CW richiede uno slot nuovo o una politica di condivisione.**

## Il fatto che rende il polling praticabile

`src/iambic.c:295-301` — la memoria dot/dash si arma **sul solo fronte di pressione** ed è un latch appiccicoso che sopravvive al rilascio:

```c
if (left) { kcwl = state; if (state) { *kmeml = 1; } }
```

Conseguenza: il polling non deve azzeccare i tempi né la durata, deve solo **non perdere nessuna chiusura di contatto**. Il pavimento dell'intervallo è la chiusura deliberata più breve, non la velocità in WPM. Rovescio: campionare troppo stretto fa emergere il rimbalzo dei contatti, che a intervalli larghi è invisibile per costruzione.

Nota correlata verificata: nei modi iambici il jitter di campionamento **non** deforma l'elemento emesso, perché `iambic.c` lo rigenera da `dot_samples`/`dash_samples`. Nel modo straight/bug sì, perché lì la durata dell'elemento è la durata dell'ingresso.

## I tre approcci, nessuno scelto

**A — Thread di polling dedicato (~2 ms).** Evoluzione di `monitor_serptt_cts_thread` in `src/rigctl.c`: stessa struttura (slot porta, contatore di generazione, stop atomico, cleanup), ma intervallo da 50 ms a ~2 ms, debounce, e consegna con **chiamata diretta a `keyer_event()`** invece di `g_idle_add()`. Confine astratto come "sorgente di eventi di paddle" così il backend Linux `TIOCMIWAIT` entra senza riscritture. *Raccomandato dall'agente, non confermato dall'utente.*

**B — Campionare in `process_control_bytes()`.** Leggere le linee dove si decodifica già il paddle della radio (`src/old_protocol.c:1675`). Nessun thread nuovo, cadenza 2,6 ms esistente. Contro: `ioctl` nel percorso caldo della RX di rete, keying ostaggio dell'arrivo pacchetti, e piattaforme che divergono di più invece che di meno.

**C — Sottosistema generico "linee seriali → ActionTable".** Come il MIDI: uno strato 1 seriale che alimenta gli strati 2-3 esistenti (`midi2.c` / `midi3.c` / `schedule_action()`) **senza toccarli**. Una linea legata a `CW_LEFT` eredita la corsia preferenziale del CW. La tabella è di 4 voci per porta (CTS, DSR, DCD, RI) contro le 128 note del MIDI. Costo marginale su A = tabella + UI + persistenza props, **non** una nuova architettura. È la forma proponibile a upstream.

## La quarta opzione, e la domanda che resta aperta

**Paddle → USB-MIDI su ESP32-S3.** Verificato che **funziona oggi con zero righe di C**:

- `src/actions.c:121-122` — `CW_LEFT` e `CW_RIGHT` sono `MIDI_KEY`, quindi compaiono nel selettore azioni
- `src/midi_menu.c:216` — il menu MIDI usa `action_dialog()` generico
- `src/midi_menu.c:1110` — il binding è persistito nel props

Vantaggi: event-driven end-to-end (il polling si sposta nel controller USB, in silicio), debounce in firmware accanto al contatto, funziona anche con Thetis/fldigi/WSJT-X. Svantaggi: nessun altro utente ha una scatoletta paddle-USB-MIDI mentre tutti hanno l'adattatore seriale, quindi decade l'argomento per andare a upstream.

**Domanda non risposta dall'utente:** l'obiettivo era *"io in aria in CW"* — e allora l'ESP32 lo risolve subito — oppure *"deskhpsdr guadagna il keying su seriale"* per sé e per gli altri? Alla domanda l'utente ha risposto *"Non correre"* e ha chiesto approfondimenti tecnici, poi la sessione si è chiusa senza tornarci.

## Correzioni fatte in sessione, per non rifare gli stessi errori

Due affermazioni di assenza si sono rivelate false. Il learning è scritto in `ce_docs/solutions/codebase-navigation/absence-claims-need-concept-search.md`; in sintesi:

1. Cercato `kAudioDevicePropertyBufferFrameSize` solo in `src/macos_audio.c` → concluso a torto che il buffer resta al default. Sta in **`src/coreaudio.c:39-41`**, target 128 frame = 2,67 ms. La stima di latenza del sidetone locale è passata da "16-25 ms" a **~9-11 ms**, cioè migliore del percorso via radio (17 ms misurati, `src/new_protocol.c:3535`).
2. Cercato `launch_serial_ptt` (nome piHPSDR) → concluso a torto che deskhpsdr non ha PTT su seriale. Si chiama **`serptt`**.

**Il paddle sulla radio e il sidetone locale sono già documentati** in `ce_docs/explainers/cw-keyer.html`, che è l'artefatto da leggere per primo prima di rientrare nel merito.

## Stato del tree

Al momento della scrittura, su `master` a `9f30362`, tutto non committato:

- `ce_docs/explainers/cw-keyer.html` — nuovo, 9 sezioni sul cablaggio del keyer e le latenze
- `ce_docs/solutions/codebase-navigation/absence-claims-need-concept-search.md` — nuovo
- `CLAUDE.md` — nuovo: topologia remote, regola `.gitignore` fuori dalle PR upstream, avvertenza di non rebasare le branch `windows*`
- `.compound-engineering/config.yaml` + `config.example.yaml` — nuovi, `docs_root: ce_docs`
- `.gitignore` — modificato, aggiunto `.context/compound-engineering/`

**Stato fragile da sapere:** i remote sono stati ricablati in questa sessione. `origin` = `iu3qez/deskhpsdr` (il fork, allineato con un push fast-forward di 736 commit), `upstream` = `dl1bz/deskhpsdr`. Le tre branch `windows`, `windows-client-server`, `windows-porting` contengono 62 commit che esistevano **solo** sul fork remoto e sono ora anche in locale: **non rebasare né force-pushare** senza chiedere.

## Continuazioni plausibili

Fork mutuamente esclusivo, da porre all'utente:

1. **Riprendere la domanda a monte** — l'ESP32-S3 lo sblocca senza codice; la feature seriale serve ancora, e per chi?
2. **Riprendere dalla scelta A/B/C** se la risposta è che la feature serve comunque a deskhpsdr.

Se si sceglie (2), la strada è: `ce-brainstorm` per chiudere Phase 2 e scrivere il Product Contract in `ce_docs/plans/`, poi `ce-plan`, poi `ce-work`.

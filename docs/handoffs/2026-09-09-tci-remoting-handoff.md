# Handoff — TCI remote extensions (deskHPSDR fork)

Data: 2026-09-09. Branch: `claude/deskhpsdr-tci-remoting` (worktree `.claude/worktrees/deskhpsdr-tci-remoting`), base `master` = `016400b` (upstream dl1bz 2.7.38). PR: https://github.com/iu3qez/deskhpsdr/pull/1 (verso `master` del fork, **non** verso dl1bz).

## Cosa esiste

Lato server deskHPSDR del progetto "stazione remota" (requisiti: `docs/brainstorms/2026-09-08-remote-station-requirements.md`; piano: `docs/plans/2026-09-08-2215-feat-tci-remote-spectrum-extensions-plan.md`; specifica di protocollo: `documentation/deskHPSDR_TCI_Remote_Extensions.md`). Tutte le 8 unità del piano sono implementate e committate:

| Unità | Contenuto | File chiave |
|---|---|---|
| U1 | modulo puro del frame `type=4` + harness | `src/tci_spectrum.{c,h}`, `tests/tci_spectrum_test.c`, target `make tci-spectrum-test` |
| U2 | mappatura pixel→Hz condivisa, hook del produttore nel ciclo di display | `rx_panadapter_get_mapping()`, `tci_rx_spectrum_block/deliver`, `tci_rx_displaying_changed` |
| U3 | `spectrum_start/stop/span`, stato per client, slot coalescente nel write path lws, eventi `spectrum_state` | `src/tci.c` |
| U4 | scala fps adattiva 20→10→5 su saturazione del socket | `tci_spectrum_ladder_*` |
| U5 | `rx_att_ex` (per ADC, tipizzato att/gain/none), `band_ex` (per titolo, band-stack, idempotente) | `src/tci.c`, `sliders_update_att_gain()`, `vfo_band_change_allowed()` |
| U6 | permessage-deflate | `build-libwebsockets.sh` (+extensions +zlib), Makefile `-lz`, `tci_lws_server()` |
| U7 | bind address | props `tci_bind_addr`, `rigctl_bind_addr` (rigctl fail-closed) |
| U8 | sonda e documentazione | `stuff/tci_spectrum_probe.py` (uv, `watch/passive/att/band/span`, `--selftest`), spec md, riga nel README |

Grammatica e layout del frame sono il **contratto** per il client TOTW (fork da fare in un repo separato, piano proprio). Indice `<rx>`: `receiver[]` per `spectrum_*` e `rx_att_ex` (ADC risolto dal receiver), VFO per `band_ex`.

## Decisioni prese (non riaprire senza motivo)

- `spectrum_span` è ritaglio software: mai zoom/pan locali.
- In TX non-duplex lo stream si sospende con evento `spectrum_state:<rx>,0;`, ripresa automatica.
- `rx_att_ex` è passthrough tipizzato per ADC; la risposta porta l'indice ADC.
- **Audio prima di tutto**: lo slot spettro è subordinato alla FIFO (audio/IQ). Con audio TCI attivo su link saturo lo spettro resta affamato: è voluto, documentato nella spec §2.4 e memorizzato. Su WAN l'audio va su Mumble (DEC-01 = A), non su TCI.
- Deflate richiede la lws compilata in locale (Homebrew e i pacchetti distro sono senza extension); i browser lo negoziano da soli, i client nativi no.
- Livello 0 della scala fps = richiesta; la scala scende solo sotto saturazione.
- Radio del banco: ANAN con schede Orion/Angelia (due ADC, attenuatore 0..31, niente gain RX). Nessuna Hermes-Lite 2.

## Verifiche fatte

- `make clean && make` su macOS (lws Homebrew senza extension): pulito, nessun warning nei file toccati.
- `make tci-spectrum-test`: 3544 controlli, 0 falliti.
- `uv run --script stuff/tci_spectrum_probe.py --selftest`: parser conforme al layout.
- Mappatura panadapter prima/dopo confrontata bit a bit su 211 680 combinazioni (harness del worker, non committato).
- Semplificazione (`ce-simplify-code`) applicata; code review (`ce-code-review`, run `run-VmYsGC`) con 3 fix applicati, 1 rilievo accettato (priorità audio), residui nella PR.

## Verifiche mancanti (servono la radio / l'host Linux)

1. Confronto traccia locale vs bin ricevuti a zoom 1 (stesso floor entro 1 dB).
2. `display_debug` con 2 receiver e un client iscritto: `load` entro +10 %.
3. WAN simulata su host Linux: `tc qdisc add dev <if> root netem delay 60ms 20ms loss 2% rate 32kbit`, sonda per 30 min: `spectrum_fps` scende a 5 e risale.
4. `lsof -i -sTCP:LISTEN` / `ss -ltunp` con `tci_bind_addr`/`rigctl_bind_addr` impostati; caso indirizzo non valido → nessun listener rigctl.
5. Sessione con `-fsanitize=address`: connessioni/disconnessioni ripetute con spettro iscritto (finestra use-after-close preesistente del pattern `tci_clients_snapshot()`, allargata dal produttore).
6. Ricompilazione lws locale (`./build-libwebsockets.sh`, serve zlib-dev) e verifica handshake con extension + banda (`ss -i`): 512 bin a 10 fps < 15 kbit/s con deflate.
7. Regressione client stock: Thetis 10 min con audio e IQ, nessun frame `type=4`; TOTW stock in browser con deflate attivo, CPU thread lws < 5 %.
8. `rx_att_ex` e `band_ex` sul banco con la sonda (`att`, `band 20`, `band 20 --next`).

## Rischi residui noti

- Use-after-close di `CLIENT` (preesistente, vedi punto 5).
- `src/tci.c` ~7300 righe; le 15 array per receiver in `CLIENT` potrebbero diventare una struct.
- `spectrum_start` durante una pausa del display può leggere `displaying` fuori mutex (race stretta, effetto: stato 1 poi 0).
- Pausa > 10 s: la scala risale di un gradino al primo tick dopo la ripresa (benigno).
- Makefile senza dipendenze dagli header: dopo merge o modifica di un `.h` serve `make clean && make`.

## Come riprendere

```bash
cd /Users/sf/Developer/deskhpsdr/.claude/worktrees/deskhpsdr-tci-remoting
git fetch origin && git status
make clean && make -j8 && make tci-spectrum-test
uv run --script stuff/tci_spectrum_probe.py watch --host <host-radio> --bins 512 --fps 10 --seconds 30 --dump-first
```

Vincoli operativi della sessione: i worktree creati dall'harness per i subagent partono da `master`, non dal branch (i worker devono copiare i file correnti e i diff vanno calcolati contro uno snapshot); `git -C` verso altri worktree è bloccato.

## Prossimi passi del progetto

1. Bench con la ANAN: punti 1, 2, 4, 8 sopra.
2. Host Linux: build con lws locale (deflate), netem, ASan (punti 3, 5, 6, 7).
3. Piano separato per il fork TOTW (CLI-01…07 dell'origin): dispatch su `type` a offset 24, handler `type=4`, `spectrum_span` al cambio zoom, Web MIDI.
4. Piano separato per il tool CW remoto (CW-01…05): MIDI virtuale `snd-virmidi`, key-on/off + PTT, fail-safe alla caduta.
5. Mumble su PipeWire per l'audio (AUD-04).
6. Nessuna PR verso dl1bz prima che il percorso Linux (e il CW seriale con `TIOCMIWAIT`) sia pronto.

# Handoff — deskHPSDR TCI remoting e fork TOTW

Data: 2026-09-10. Sostituisce `docs/handoffs/2026-09-09-tci-remoting-handoff.md`, che descriveva uno stato ormai superato: la PR era ancora aperta, upstream non era mergiato, il client non esisteva.

## Stato dei repository

**`iu3qez/deskhpsdr`**, branch `feat/tci-bind-address-ui`. Il branch e' stato rinominato il 2026-09-10: il nome autogenerato dalla sessione, `claude/riprendiamo-da-handoff-a19a6c`, non esiste piu' ne' in locale ne' su origin. La directory del worktree conserva il vecchio nome, perche' rinominare il branch non la tocca.

La PR #1 con le estensioni TCI e' **mergiata** in `origin/master` (`8e84a99`). Il branch di lavoro `claude/deskhpsdr-tci-remoting` e' storia chiusa. Su questo branch ci sono, sopra al merge della PR:

| Commit | Contenuto |
|---|---|
| `75e5507` | il vecchio handoff |
| `4e9e6d7` | campi bind address nel menu CAT/TCI, piu' il fail-closed su TCI |
| `128fe04` | merge di `upstream/master` a `05c84a3`, sette commit di dl1bz |
| `d70c5f6` | questo handoff |

Il `master` locale e' rimasto indietro a `2bbe760` e diverge da `origin/master`. Non e' un problema in se', ma prima o poi va riconciliato: tutto quello che serve sta su questo branch.

**`iu3qez/thetis-on-the-web`**, fork creato il 2026-09-09, branch `plan/deskhpsdr-client`, HEAD `2bb9f51`. Clone in `~/Developer/thetis-on-the-web`, con `upstream` verso `n9bc/thetis-on-the-web`. `main` e' allineato a upstream (`c147edf`). Contiene solo il piano, nessun codice.

## Cosa e' successo nell'ultima sessione

**Campi bind address nel menu.** `tci_bind_addr` e `rigctl_bind_addr` esistevano come proprieta' ma non avevano nessun widget: si potevano impostare solo editando il props file ad applicazione chiusa, perche' `saveProperties()` riscrive il file intero all'uscita. Ora ogni server ha una riga `Bind` sotto la sua porta, editabile solo a server spento come le porte stesse, con un'icona di avviso se `inet_pton` rifiuta il valore.

**Fail-closed su TCI.** Questo era il difetto vero. Senza `LWS_SERVER_OPTION_FAIL_UPON_UNABLE_TO_BIND`, libwebsockets parcheggia un vhost la cui interfaccia non si risolve nella sua `no_listener_vhost_list` e ritorna 1; `lws_create_vhost` fallisce solo su valori negativi, quindi `lws_create_context` riusciva e deskHPSDR dichiarava avviato un server **senza socket in ascolto**. La direzione era quella sicura, ma contraddiceva quello che la specifica prometteva al paragrafo 6. Ora TCI si comporta come rigctl.

**Merge di upstream.** Sette commit, nessun conflitto, nostre modifiche a `radio.c`, `sliders.c` e `Makefile` tutte intatte. Due cambiamenti da tenere a mente:

- dl1bz e' passato da `-O3` a `-O2 -flto` su macOS, sia per deskHPSDR sia per WDSP. Il binario cala di circa 950 kB. Cade sul percorso caldo che abbiamo aggiunto noi, il produttore dello spettro nel ciclo di display: se al bench il carico sembra piu' alto del previsto, guardare qui prima del nostro codice.
- `optimize_for_touchscreen` si chiama ora `touch_ui`; Dark Theme e Touch UI sono i default.

**Fork TOTW e piano.** Il piano e' in `docs/plans/2026-09-09-feat-deskhpsdr-client-plan.md` del repo nuovo, otto unita'.

## Verifiche fatte

- `make clean && make` dopo il merge: pulito, gli unici due warning sono preesistenti in `rx_menu.c` su variabili inutilizzate.
- `make tci-spectrum-test`: 3544 controlli, 0 falliti, prima e dopo il merge.
- **Bind address provato sul banco con la radio accesa.** Con `tci_bind_addr=127.0.0.1` il socket e' `TCP 127.0.0.1:40001 (LISTEN)` e non `*:40001`; connessione a `127.0.0.1:40001` riuscita, a `192.168.1.199:40001` rifiutata. Il log mostra la transizione da `all interfaces` a `127.0.0.1`. Persistenza nel props verificata. L'operatore ha confermato provato anche il resto: icona di avviso e campo rigctl.

## Verifiche ancora mancanti (servono la radio o l'host Linux)

Invariate rispetto al handoff precedente, tranne il punto sul bind che e' stato chiuso:

1. Confronto traccia locale contro bin ricevuti a zoom 1, stesso floor entro 1 dB.
2. `display_debug` con due receiver e un client iscritto: `load` entro il 10 % in piu'.
3. WAN simulata su host Linux con `netem` a 60 ms, 2 % di perdita, 32 kbit/s, per 30 minuti: `spectrum_fps` scende a 5 e risale.
4. Sessione con `-fsanitize=address`: connessioni e disconnessioni ripetute con lo spettro iscritto. Resta la finestra use-after-close preesistente del pattern `tci_clients_snapshot()`, allargata dal produttore.
5. Ricompilazione locale di libwebsockets con le extension e verifica della banda con deflate: 512 bin a 10 fps sotto i 15 kbit/s.
6. Regressione con client stock: Thetis per 10 minuti con audio e IQ senza vedere un frame `type=4`.
7. `rx_att_ex` e `band_ex` sul banco con la sonda.

## Rischi residui noti

- Use-after-close di `CLIENT`, preesistente, vedi punto 4.
- `src/tci.c` sfiora le 7300 righe; le quindici array per receiver in `CLIENT` vorrebbero essere una struct.
- `spectrum_start` durante una pausa del display puo' leggere `displaying` fuori mutex. Corsa stretta, effetto: stato 1 e poi 0.
- Il Makefile non traccia le dipendenze dagli header: dopo un merge che tocca un `.h` serve `make clean && make`, altrimenti oggetti compilati contro un layout di struct vecchio.

## Come riprendere

```bash
cd /Users/sf/Developer/deskhpsdr/.claude/worktrees/riprendiamo-da-handoff-a19a6c
git fetch origin && git fetch upstream && git status
make clean && make -j8 && make tci-spectrum-test
```

Per il client:

```bash
cd ~/Developer/thetis-on-the-web && git fetch upstream && git status
```

Vincoli operativi della sessione: i worktree creati dall'harness per i subagent partono da `master`, non dal branch; `git -C` verso altri worktree dello stesso repo e' bloccato, mentre verso un repo diverso funziona.

## Prossimi passi

1. **Ricognizione TOTW stock contro deskHPSDR**, prima di scrivere una riga di client. Serve a confermare il clic a 93,75 Hz e il pannello IQ piatto. Il piano dice di fermarsi se non si manifestano.
2. Unita' C1, C2, C3 del piano client: dispatch rigoroso, sample rate configurabile, handler dello stream a bin.
3. Bench con la ANAN per i punti 1, 2 e 7 delle verifiche mancanti.
4. Host Linux per i punti 3, 4, 5 e 6.
5. Piano separato per il tool CW remoto (CW-01…05): MIDI virtuale `snd-virmidi`, key-on/off piu' PTT, fail-safe alla caduta del link.
6. Mumble su PipeWire per l'audio (AUD-04).
7. Nessuna PR verso dl1bz finche' il percorso Linux, e il CW seriale con `TIOCMIWAIT`, non sono pronti.

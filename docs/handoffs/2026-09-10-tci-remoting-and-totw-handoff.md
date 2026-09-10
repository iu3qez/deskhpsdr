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

### Sessione di banco del 2026-09-10

Prima di iniziare, due ore perse su un falso allarme che vale la pena non ripetere: `rx_att_ex` e `band_ex` andavano in timeout perche' il binario in esecuzione era quello del **checkout principale**, compilato il 5 settembre e privo di ogni estensione. Il sorgente era gia' riallineato, il binario no. Verificare sempre con `lsof -p <pid> -a -d txt` quale eseguibile stia davvero girando.

- **`rx_att_ex` chiuso.** Interrogazione, scrittura e lettura su due ADC distinti. Con RX1 su ADC0 e RX2 su ADC1 i due receiver riportano valori di attenuazione **diversi**, il che dimostra che il comando legge lo stato per ADC e non uno globale. Portando entrambi i receiver su ADC0 riportano lo stesso valore, come atteso. Range 0..31 passo 1, `kind=att`, nessun gain: coerente con le schede Orion/Angelia.
- **`band_ex`** verificato in interrogazione e in cambio banda. Manca solo la variante `--next` sul band stack.
- **Base del carico a due receiver**, binario nuovo, entrambi i panadapter attivi, 30 fps configurati:

| | load | avg |
|---|---|---|
| RX1 | 18,6 - 18,9 % | 6,21 - 6,31 ms |
| RX2 | 16,5 - 17,5 % | 5,50 - 5,84 ms |

Somma intorno al 35-36 %, `late` a zero su tutte le finestre. **Attenzione:** `load` e' per receiver, le due righe vanno sommate perche' entrambi i callback girano sulla stessa main loop GTK. E `rendered` non conta i disegni ma le volte che `rx_get_pixels()` ha restituito dati: con panadapter e cascata spenti su un receiver, quel receiver costa 0,29 ms e `rendered` resta alto lo stesso.

Un'osservazione utile per misure future: con panadapter e cascata spenti su RX2, il costo del nostro produttore si legge quasi puro, perche' il rumore di fondo scende da 6 ms a 0,29. Con i disegni accesi lo stesso incremento resta annegato.

## Verifiche ancora mancanti (servono la radio o l'host Linux)

Chiusi il bind address e `rx_att_ex`. Restano:

1. Confronto traccia locale contro bin ricevuti a zoom 1, stesso floor entro 1 dB. **Non ancora tentato: la sottoscrizione allo spettro non e' mai partita.**
2. `display_debug` con due receiver e un client iscritto: `load` entro il 10 % in piu' della somma 35-36 % misurata come base.
2b. `band_ex` con `--next`, l'unica parte del comando non provata.
3. WAN simulata su host Linux con `netem` a 60 ms, 2 % di perdita, 32 kbit/s, per 30 minuti: `spectrum_fps` scende a 5 e risale.
4. Sessione con `-fsanitize=address`: connessioni e disconnessioni ripetute con lo spettro iscritto. Resta la finestra use-after-close preesistente del pattern `tci_clients_snapshot()`, allargata dal produttore.
5. Ricompilazione locale di libwebsockets con le extension e verifica della banda con deflate: 512 bin a 10 fps sotto i 15 kbit/s.
6. Regressione con client stock: Thetis per 10 minuti con audio e IQ senza vedere un frame `type=4`.

## Rischi residui noti

- Use-after-close di `CLIENT`, preesistente, vedi punto 4.
- `src/tci.c` sfiora le 7300 righe; le quindici array per receiver in `CLIENT` vorrebbero essere una struct.
- `spectrum_start` durante una pausa del display puo' leggere `displaying` fuori mutex. Corsa stretta, effetto: stato 1 e poi 0.
- Il Makefile non traccia le dipendenze dagli header: dopo un merge che tocca un `.h` serve `make clean && make`, altrimenti oggetti compilati contro un layout di struct vecchio.

## Debiti di funzionalita' di deskHPSDR

Emersi lavorando, non sono nostri e non sono stati corretti tranne dove indicato. Hanno tutti la stessa forma: **il componente fallisce verso il silenzio invece che verso l'errore**, e l'operatore non ha modo di distinguere "non funziona" da "non c'e'".

1. **Il pannello P2 ADC/DDC e' mostrato su radio dove non ha effetto.** Il pulsante compare per qualsiasi dispositivo in protocollo 2, l'unica condizione in `new_menu.c` e' `protocol == NEW_PROTOCOL`. Ma `p2_receiver_adc_assignment()` in `new_protocol.c` applica la matrice **solo** per `NEW_DEVICE_HERMES` e `NEW_DEVICE_ANGELIA`. Su Orion, Orion2, Saturn e G2 la funzione viene chiamata con indice DDC negativo, salta la matrice e usa `receiver[i]->adc`. Quindi su quelle radio si puo' configurare l'intero pannello senza che cambi nulla, e la nota interna avverte solo che Hermes ha un ADC solo. Il posto giusto e' il menu Receive, voce "Select ADC", che infatti compare proprio quando la matrice **non** e' onorata.

   Corollario da ricordare: l'indice DDC non e' l'indice del receiver. Su Angelia, Orion, Orion2 e Saturn `receiver[i]` sta su **DDC(i+2)**; solo su Hermes vale DDC(i). E due receiver non possono condividere un DDC: cio' che si condivide e' l'ADC.

2. **TCI ignora in silenzio i comandi che non conosce.** Non esiste una risposta di errore per comando sconosciuto: la traccia c'e' solo nel log del server e solo con `rigctl_debug` acceso. La casella "Enable TCI Debug" del menu accende `tci_debug`, che e' un flag diverso e non stampa i comandi ricevuti. E' quello che ha reso difficile capire che stava girando un binario vecchio.

3. **Una build vecchia cancella dal props le chiavi che non conosce.** `radio_save_state()` fa `clearProperties()` e riscrive solo le chiavi note al binario. Avviare una build precedente su una configurazione nuova ne distrugge le impostazioni all'uscita, senza avviso. E' successo davvero: `tci_bind_addr` e' sparito dal props.

4. **libwebsockets non apriva il listener mentre il programma dichiarava di essere partito.** Gia' corretto in `4e9e6d7` con `LWS_SERVER_OPTION_FAIL_UPON_UNABLE_TO_BIND`. Resta qui come quarto esemplare della stessa famiglia.

## Issue upstream da tenere d'occhio

**`dl1bz/deskhpsdr` #203, "Audio stuttering, bit/sample rate issue maybe?"** — aperta, etichettata Bug / using macOS / Under active investigation, aggiornata il 2026-09-10. Clic e scoppiettii in RX e TX su macOS Tahoe, ANAN 7000 DLE mkII in protocollo 2, con il sistema a 48 k. Ci riguarda per tre motivi: e' la stessa piattaforma e lo stesso protocollo del nostro banco, il merge che abbiamo appena assorbito contiene `9a8172f` "Improve CoreAudio microphone buffer drift handling" che potrebbe essere il lavoro in corso su questa issue, e se durante il bench si sentono clic nell'audio **il primo sospettato non e' il nostro codice**.

**`n9bc/thetis-on-the-web`**, nove issue aperte, tre pertinenti al piano del client:

- **#8, "https server required for PTT function"**: conferma dall'utenza che il requisito CLI-09, cioe' l'unita' C8, e' un bisogno gia' segnalato e non una nostra aggiunta teorica.
- **#9, "RX audio lags"**: il ritardo audio peggiora progressivamente e si azzera solo spegnendo e riaccendendo l'audio RX. E' lo scheduler naive gia' descritto nei requisiti. Il nostro piano **non** lo affronta, perche' CLI-08 e' fuori scopo con DEC-01 = A. Il fork eredita il problema.
- **#12, "There is some humming sound in the CW carriers"**: da verificare, ma potrebbe essere lo stesso difetto che abbiamo diagnosticato per CLI-01. Se l'header da 8 byte assunto per Thetis fosse sbagliato anche per Thetis, i byte di header residuo verrebbero riprodotti come campioni, e a 512 campioni su 48 kHz il risultato e' un ronzio a 93,75 Hz. **E' un'ipotesi, non un fatto**: serve un dump di un frame audio reale di Thetis per confermarla. Se regge, la nostra correzione di C1 chiude anche una issue di n9bc, ed e' un buon motivo per aprire un dialogo con lui prima di divergere troppo.

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

# deskHPSDR TCI Remote Extensions

This document specifies a set of opt-in additions to deskHPSDR's TCI 2.0
server: a binary spectrum stream, two CAT-like commands, WebSocket
compression, and a configurable bind address. It is written for a reader
who has never seen deskHPSDR's source: every offset, constant and reply
string below is meant to match `src/tci_spectrum.h` and `src/tci.c`
byte for byte and character for character. If it does not, the source is
authoritative.

## 1. Purpose and scope

deskHPSDR's TCI server accepts WebSocket text commands terminated by `;`
on TCP port 40001 by default, plus binary audio and I/Q frames. These
extensions add:

- a binary spectrum stream (`type=4` frames) a client can subscribe to,
  intended for a remote panadapter over a constrained link (WAN,
  WireGuard) at well under 50 kbit/s;
- `rx_att_ex` and `band_ex`, two additional CAT-like text commands for
  attenuation/gain and band changes;
- optional `permessage-deflate` WebSocket compression;
- a configurable bind address for the TCI and rigctl TCP listeners.

**Everything here is opt-in per client.** A client that never sends a
`spectrum_*` command receives no `type=4` frame and no `spectrum_*`
text message, ever — not even when another client on the same server is
actively streaming spectrum data. No existing command, reply or event is
changed, renamed or reordered. Stock TCI clients (Thetis, ExpertSDR-like
clients, ordinary TOTW-style browser clients) see identical behavior to
a deskHPSDR build without these extensions, aside from the WebSocket
compression a browser negotiates by default in its handshake (section 5).

**There is no authentication.** TCI never had one, and this extension
adds none: any client that can open a WebSocket to port 40001 can read
the spectrum, change attenuation and change bands. Do not expose the
port to the open Internet. Run it inside a private network, and for
remote access use WireGuard or an equivalent VPN. The bind address
(section 6) is the only access control available, and it is a network
perimeter control, not authentication.

## 2. Spectrum stream

### 2.1 Negotiation commands

The subscription itself is the negotiation — there is no separate
"enable" step. `<rx>` is the index into deskHPSDR's `receiver[]` array
(0 or 1 on a two-receiver radio).

| Command | Reply |
|---|---|
| `spectrum_start:<rx>[,<bins>[,<fps>]];` | `spectrum_start:<rx>,<bins>,<fps>;` then `spectrum_state:<rx>,<0\|1>;` |
| `spectrum_stop:<rx>;` | `spectrum_stop:<rx>;` |
| `spectrum_span:<rx>[,<low>,<high>];` | `spectrum_span:<rx>,<low>,<high>;` or `spectrum_span:<rx>,<low>,<high>,pending;` or `spectrum_span:<rx>,error;` |

Missing arguments to `spectrum_start` mean the documented defaults, so
`spectrum_start:0;` is a complete subscription request, not a query:
512 bins, 10 fps. The reply always reports the values actually applied
— the bin count clamped to `[16, 4096]`, and the fps rounded down to a
value the server can actually serve (section 2.4). A bare
`spectrum_start:0;` therefore commonly replies `spectrum_start:0,512,10;`.

`spectrum_stop` does not require the receiver to still exist, so a
client can always unsubscribe from a receiver that has since been
removed.

`spectrum_span` with no `<low>,<high>` is a plain query of the current
span. `0,0` (as either the request or a query before any frame has
gone out) means "the whole available span". If the requested span has
not produced a frame yet, the reply ends in `,pending;` instead of
being a bare confirmation, and reports the last requested (not yet
effective) edges; once a frame goes out with that span the plain,
non-pending reply takes over. `low >= high` (and not both zero) is
rejected with `spectrum_span:<rx>,error;` and has no effect on the
stored request.

### 2.2 Events

Two asynchronous messages are sent only to clients subscribed to the
receiver in question, never broadcast:

- `spectrum_state:<rx>,<0|1>;` — the receiver's display stopped (`0`)
  or resumed (`1`) updating. This covers TX on a non-duplex radio and
  removal of a second receiver: no frames are sent while state is `0`,
  the subscription is preserved across the pause, and no re-subscription
  is needed when state returns to `1`. `spectrum_start`'s reply always
  includes one of these immediately, so a client can tell "paused" apart
  from "dead link" from the first exchange.
- `spectrum_fps:<rx>,<fps>;` — the effective fps served to this client
  changed. This is the mechanism behind the adaptive ladder in 2.4.

### 2.3 Frame layout

A frame is a single WebSocket binary message: the existing 64 byte TCI
stream header (unchanged layout, shared with the audio and I/Q streams),
followed by a 32 byte spectrum-specific prefix, followed by `length`
one-byte bins. Everything is little-endian.

**Header (64 bytes, offset 0):**

| Offset | Type | Field | Value for this stream |
|---|---|---|---|
| 0 | uint32 | `receiver` | index into `receiver[]` |
| 4 | uint32 | `sample_rate` | the receiver's sample rate, Hz |
| 8 | uint32 | `format` | `4` — quantized uint8 bins |
| 12 | uint32 | `codec` | `0` |
| 16 | uint32 | `crc` | `0` |
| 20 | uint32 | `length` | number of bins that follow |
| 24 | uint32 | `type` | `4` — spectrum stream |
| 28 | uint32 | `channels` | `1` |
| 32 | uint32[8] | `reserv` | `0` |

**Prefix (32 bytes, offset 64):**

| Offset (from prefix start) | Type | Field | Note |
|---|---|---|---|
| 0 | uint16 | `version` | `1` |
| 2 | uint16 | `flags` | bit0 = span clipped to the available one, bit1 = full available span |
| 4 | uint32 | `seq` | per client and receiver, restarts at 0 on every `spectrum_start` |
| 8 | int64 | `low_hz` | frequency of the left edge of bin 0 |
| 16 | int64 | `high_hz` | frequency of the right edge of the last bin |
| 24 | float32 | `floor_db` | frame minimum, rounded down to a 0.5 dB step |
| 28 | float32 | `scale_db` | `0.5` |

**Bins (offset 96, `length` bytes):** one `uint8` per bin.

Frequencies are absolute `int64` Hz, not offsets from a `vfo:` message:
a float32 has a 16 Hz ULP at 144 MHz, and TCI's own `vfo:` replies
report `ctun_frequency` under CTUN, which is not enough for a client to
derive the spectrum's center on its own. Every frame is therefore
self-describing — a client does not need to track VFO or sample-rate
changes to keep the axis correct; the next frame's `low_hz`/`high_hz`
already reflects them (section 2.6).

`type` is `4` deliberately: the ExpertSDR TCI specification this server
otherwise follows only defines stream types 0-3 (audio, etc.). If
upstream ExpertSDR later assigns `4` to something else, a client only
has to remap this one constant.

### 2.4 Reconstructing a bin, fps rules

```
dbm = floor_db + q * scale_db      # q is the raw uint8 bin value
```

`q == 255` means saturation (the true value is at or above what 255
can represent at this frame's floor/scale), not necessarily an exact
dBm reading.

The dB values already include the local panadapter's attenuation and
preamp correction — the same offset the local display applies — so a
frame's floor and peaks line up with the local panadapter trace, not
with a raw, uncorrected level.

fps is negotiated but capped: `fps_eff` never exceeds the receiver's own
local display fps, and it is chosen so it divides evenly into it (e.g.
requesting 20 fps against a 30 fps local display yields 15, not 20).
The reply to `spectrum_start` always carries the true `fps_eff`.

On top of that static ceiling, the server runs an **adaptive ladder**:
when the client falls behind and a pending frame has to be replaced in
its output slot three times within one second, the server steps the
served rate down (20 -> 10 -> 5 fps); after ten consecutive quiet
seconds (no replacements) it steps back up, one step at a time, up to
the originally requested/negotiated ceiling. The trigger is socket
saturation — a write not completing within one producer period — not
RTT or packet loss as such, though on a lossy, bandwidth-limited link
those manifest as the same symptom. Every step, in either direction,
sends `spectrum_fps:<rx>,<fps>;` to the affected client only.

A frame that is still waiting when a new one is produced is *replaced*,
never queued behind its predecessor: a slow client always gets the
freshest data, at the cost of gaps in `seq`. A gap in `seq` is exactly
the count of replaced frames, and is the signal the probe (section 7)
reports as "seq gaps".

**Priority is audio first, always.** The slot is written only when the
client's ordinary queue (text replies, RX audio, I/Q) is empty. If that
client also runs the TCI audio or I/Q stream over a link that cannot
carry it, the queue never drains, the slot is never written, the ladder
steps down and no spectrum frame arrives until the audio catches up.
This is deliberate: a stalled waterfall for a few seconds is acceptable,
a dropped audio frame is not, so the server never interleaves spectrum
frames ahead of audio. On a bandwidth-limited WAN keep audio on a
separate transport (for example Mumble) and use TCI for commands and
spectrum only.

### 2.5 Span rules

`spectrum_span:<rx>,<low>,<high>;` crops the stream to a Hz range,
per client and receiver. This is a pure software crop on the server
side: it does **not** call into the GUI's zoom/pan machinery and has no
effect on the local panadapter's zoom or pan.

- `0,0` means "the whole available span" (also the default before any
  `spectrum_span` has been sent).
- The requested range is intersected with the receiver's actually
  available span on every frame; the frame's `low_hz`/`high_hz` report
  what was actually sent, which may be narrower than requested if the
  request extended past the edges (flag bit0, "clipped", is set in that
  case).
- If the requested range does not intersect the available span at all
  (empty, inverted, or fully outside), **no frame is sent** until the
  receiver's tuning or sample rate brings the two back into overlap; the
  subscription and the stored request are both preserved meanwhile.
- Exactly one array is sent per frame; the server never emits more than
  one span per client per frame period.

### 2.6 TX, non-duplex, zoom and sample-rate changes

On a non-duplex radio going to TX, or when a second receiver is
removed, the affected receiver's display stops updating; subscribed
clients get `spectrum_state:<rx>,0;` and no frames until
`spectrum_state:<rx>,1;` announces the resume (section 2.2). No
`spectrum_start` is needed after a resume.

Changing the local zoom, pan or the receiver's sample rate does not
require any client action either: because every frame carries its own
`low_hz`/`high_hz`/`sample_rate`, the client simply redraws its axis
from the next frame it receives — there is no separate "span changed"
event to listen for, and no round trip needed to re-fetch the new span.

### 2.7 Bandwidth

At the negotiation defaults (512 bins, 10 fps), each frame is
`64 + 32 + 512 = 608` bytes, i.e. roughly `608 * 8 * 10 ≈ 49` kbit/s of
raw payload before any transport overhead or compression — the
"under 50 kbit/s" figure referenced in the plan's goal. With
`permessage-deflate` negotiated (section 5), this is typically well
below that; the exact figure depends on the entropy of the current
scene and should be measured on the link in question, not assumed.

## 3. `rx_att_ex` — attenuation / RF gain

Attenuation and RF gain belong to the ADC, not to the receiver: on a
single-ADC radio (Hermes-Lite 2, Hermes, ANAN-10/100) both receivers
share ADC 0 and therefore the same value; only two-ADC radios
(ANAN-100D/200D/7000/8000, G2) have independent per-receiver values.
`<rx>` is a `receiver[]` index; the server resolves the ADC it is
attached to and reports that index back in the reply so the client
never has to guess which ADC a change actually affected.

| Command | Reply |
|---|---|
| `rx_att_ex:<rx>;` (query) | `rx_att_ex:<rx>,<kind>,<value>,<min>,<max>,<step>,<adc>;` |
| `rx_att_ex:<rx>,<value>;` (set) | same reply, with the value actually applied |

`<kind>` is one of:

- `att` — a stepped attenuator, range `0..31`, step `1` (radios where
  `have_rx_att` is true);
- `gain` — a continuous RF gain, range `[adc[].min_gain, adc[].max_gain]`,
  step `1` (radios where `have_rx_gain` is true instead — this is the
  Hermes-Lite 2 case, e.g. `-12..48`);
- `none` — neither is available; a set request is ignored and the query
  reply is returned unchanged.

A request outside `[<min>, <max>]`, or blocked by set-lock, is ignored
and answered with the current (unchanged) value — there is no separate
error reply for `rx_att_ex`, unlike `spectrum_span` and `band_ex`.

## 4. `band_ex` — band change through the band-stack

`<rx>` here is a **VFO index**, not a receiver index — 0 = VFO A, 1 =
VFO B, exactly as `tci_set_vfo()` already uses it. The band is
addressed by its title (`"20"`, `"40"`, `"160"`, `"GEN"`, ...), which is
stable across IARU regions, unlike the underlying band enum index.

| Command | Reply |
|---|---|
| `band_ex:<rx>;` (query) | `band_ex:<rx>,<title>;` |
| `band_ex:<rx>,<title>;` (set, same band) | `band_ex:<rx>,<title>;` — no-op, frequency unchanged |
| `band_ex:<rx>,<title>,next;` | `band_ex:<rx>,<title>;` — advances the band's band-stack to its next entry |
| any of the above, band unknown or out of the radio's frequency range | `band_ex:<rx>,<title>,error;` |

Setting the band the VFO is already on, without `next`, is defined as a
no-op that only reports the current band — it does not touch the
band-stack position. `next` is the only way to step the band-stack
forward, and only takes effect together with a title (setting the
current band with `next` steps that band's stack; there is no
"next" without a title). Before applying a band change, the server
checks that the destination band-stack entry's frequency is within the
radio's tuning limits, to avoid landing in the partially-applied state
`vfo_band_changed()` itself would leave behind if it had to give up
partway through.

## 5. `permessage-deflate`

The server offers RFC 7692 `permessage-deflate` compression
(`client_no_context_takeover; client_max_window_bits`) in the WebSocket
handshake to every connecting client, if the libwebsockets it was built
against supports it. It is purely a handshake negotiation: there is no
new code path for any client to opt into, and no reply format changes.

- Browsers (including a stock TOTW-style client) offer and accept
  `permessage-deflate` by default, so they get it on every frame —
  audio and I/Q included, not just spectrum — automatically, with no
  visible change.
- Native clients (Thetis, Qt-based clients) generally do not offer the
  extension, so they connect exactly as before: R1 (no behavior change
  for a client that does not opt in) holds for them without exception.
- A deskHPSDR build against a libwebsockets without extension support
  compiles and runs unchanged; it logs that `permessage-deflate` is not
  available and serves every client uncompressed, as today.

## 6. Bind address

Two configuration properties restrict which interface deskHPSDR's
network servers listen on:

- `tci_bind_addr` — the TCI WebSocket server (default port 40001);
- `rigctl_bind_addr` — the rigctl TCP (TS-2000 emulation) server.

Both are edited in the `CAT/TCI` menu, on the `Bind` line below the
server they belong to, and are stored in the radio's props file. Like
the port controls next to them, each field is editable only while its
server is switched off. A value the C library cannot parse as an IPv4
address is marked with a warning icon as you type.

Both default to the empty string, meaning "all interfaces", identical
to deskHPSDR's behavior without this extension. Set either to a local
IP address (e.g. a WireGuard interface's address) to restrict that
listener to it. libwebsockets also accepts an interface *name* here:
`lws_interface_to_sa()` first matches the string against `ifa_name`
from `getifaddrs()` and only falls back to parsing it as a numeric
address, so `wg0` and `10.0.0.1` are both valid for `tci_bind_addr`.
The rigctl server takes IPv4 addresses only.

Because TCI has no authentication (section 1), the bind address is the
only access control available for it — a configured but unparsable
address therefore fails closed (the listener does not start and an
error is logged), rather than silently falling back to "all
interfaces". For TCI this requires
`LWS_SERVER_OPTION_FAIL_UPON_UNABLE_TO_BIND`: without it libwebsockets
parks the vhost on its deferred no-listener list and still reports a
successfully created context, which would leave the server up with no
listening socket and no visible error.

## 7. The Python probe

`stuff/tci_spectrum_probe.py` is a standalone script (PEP 723 `uv`
header, `websockets` dependency) used to exercise every command and
event in this document from the command line, without a GUI client.

```
uv run --script stuff/tci_spectrum_probe.py --host <host> watch
uv run --script stuff/tci_spectrum_probe.py --selftest
```

Subcommands: `watch` (subscribe and print live fps/kbit-s/span/seq-gap
statistics, optionally cropped with `--span`), `passive` (assert that a
client which never subscribes sees no spectrum data at all — the check
behind AE1), `att` (query/set `rx_att_ex`), `band` (query/set
`band_ex`), and `span` (query/set `spectrum_span` without a
subscription). `--selftest` builds a frame from known bytes and parses
it back, proving the parser against the layout in section 2.3 without
needing a radio. Run `uv run --script stuff/tci_spectrum_probe.py --help`
(or `<subcommand> --help`) for the full option list and more examples.

## 8. Compatibility notes

- `type=4` and `format=4` were chosen because the ExpertSDR TCI
  specification this server otherwise follows only assigns 0-3. A
  client should dispatch incoming binary frames on the header's `type`
  field alone (offset 24) and ignore any frame whose type it does not
  recognize, rather than assuming a fixed set of stream types — this is
  exactly what lets a deskHPSDR-aware client add spectrum support
  without breaking on servers, or other stream types, it does not know
  about.
- Nothing described here changes the on-the-wire format of the existing
  audio or I/Q streams, or of any other TCI text command.
- Independently of these extensions, narrow FM is reported by
  `modulation` and advertised in `modulations_list` as `NFM`, the name
  the TCI specification gives it. Earlier builds answered `FM` and
  advertised `FMN`; `nfm`, `fm` and `fmn` are all accepted on input.

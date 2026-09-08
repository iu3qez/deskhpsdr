#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.11"
# dependencies = ["websockets>=12"]
# ///
"""
deskHPSDR TCI spectrum probe.

Bench tool for the deskHPSDR TCI remote extensions: the type=4 binary
spectrum stream, spectrum_span, rx_att_ex and band_ex. It is the
instrument used to verify the plan's acceptance examples (AE1-AE7)
without a GUI client. The wire contract it implements is documented in
documentation/deskHPSDR_TCI_Remote_Extensions.md and in
src/tci_spectrum.h; keep the two in sync.

Examples
--------
    tci_spectrum_probe.py --host radio.local watch --seconds 30 --dump-first
    tci_spectrum_probe.py watch --span 14000000 14350000     # AE3
    tci_spectrum_probe.py passive --seconds 30                # AE1
    tci_spectrum_probe.py att            # query rx_att_ex
    tci_spectrum_probe.py att 12         # set it            # AE5
    tci_spectrum_probe.py band 20                             # AE6
    tci_spectrum_probe.py band 20 --next
    tci_spectrum_probe.py span 0 0       # full span, no subscription needed
    tci_spectrum_probe.py --selftest     # prove the parser, no radio needed
"""

import argparse
import asyncio
import struct
import sys
import time
from dataclasses import dataclass
from typing import Optional

import websockets

# --------------------------------------------------------------------------
# Wire contract -- must match src/tci_spectrum.h and src/tci_audio.h byte
# for byte. Re-grep those files if this ever looks wrong.
# --------------------------------------------------------------------------

HEADER_BYTES = 64          # TCI_STREAM_HEADER, 8 x uint32 + reserv[8]
PREFIX_BYTES = 32          # TCI_SPECTRUM_PREFIX
TYPE_SPECTRUM = 4          # header "type": spectrum stream
FORMAT_U8 = 4              # header "format": quantized uint8 bins
FLAG_CLIPPED = 0x0001      # requested span clipped to the available one
FLAG_FULL_SPAN = 0x0002    # frame carries the whole available span
SCALE_DB = 0.5             # TCI_SPECTRUM_SCALE_DB

# receiver, sample_rate, format, codec, crc, length, type, channels
_HEADER_STRUCT = struct.Struct("<8I")
# version, flags, seq, low_hz, high_hz, floor_db, scale_db
_PREFIX_STRUCT = struct.Struct("<HHIqqff")

assert _HEADER_STRUCT.size == 32          # + reserv[8] (32 bytes) = HEADER_BYTES
assert _PREFIX_STRUCT.size == PREFIX_BYTES


@dataclass
class SpectrumFrame:
    receiver: int
    sample_rate: int
    seq: int
    low_hz: int
    high_hz: int
    floor_db: float
    scale_db: float
    flags: int
    bins: bytes

    @property
    def nbins(self) -> int:
        return len(self.bins)

    def dbm(self, index: int) -> float:
        q = self.bins[index]
        return self.floor_db + q * self.scale_db

    @property
    def clipped(self) -> bool:
        return bool(self.flags & FLAG_CLIPPED)

    @property
    def full_span(self) -> bool:
        return bool(self.flags & FLAG_FULL_SPAN)


def parse_frame(data: bytes) -> Optional[SpectrumFrame]:
    """Parse one binary WebSocket message. Returns None for anything that
    is not a type=4 spectrum frame (audio/IQ frames, truncated data, ...);
    callers must check the type before touching the rest of the layout."""
    if len(data) < HEADER_BYTES + PREFIX_BYTES:
        return None

    (receiver, sample_rate, fmt, _codec, _crc, length,
     header_type, _channels) = _HEADER_STRUCT.unpack_from(data, 0)

    if header_type != TYPE_SPECTRUM:
        return None

    if len(data) < HEADER_BYTES + PREFIX_BYTES + length:
        return None

    version, flags, seq, low_hz, high_hz, floor_db, scale_db = \
        _PREFIX_STRUCT.unpack_from(data, HEADER_BYTES)
    bins_start = HEADER_BYTES + PREFIX_BYTES
    bins = data[bins_start:bins_start + length]

    return SpectrumFrame(
        receiver=receiver, sample_rate=sample_rate, seq=seq,
        low_hz=low_hz, high_hz=high_hz, floor_db=floor_db,
        scale_db=scale_db, flags=flags, bins=bins,
    )


def build_frame_for_test(receiver=0, sample_rate=192000, seq=42,
                          low_hz=14_000_000, high_hz=14_192_000,
                          floor_db=-120.0, bins=(0, 1, 2, 255, 128)) -> bytes:
    """Build one on-the-wire frame from known values, the same layout
    tci_spectrum_serialize() writes in src/tci_spectrum.c."""
    nbins = len(bins)
    header = bytearray(HEADER_BYTES)
    _HEADER_STRUCT.pack_into(header, 0, receiver, sample_rate, FORMAT_U8,
                              0, 0, nbins, TYPE_SPECTRUM, 1)
    # reserv[8] (offset 32..63) stays zero
    prefix = _PREFIX_STRUCT.pack(1, FLAG_FULL_SPAN, seq, low_hz, high_hz,
                                  floor_db, SCALE_DB)
    return bytes(header) + prefix + bytes(bins)


def selftest() -> int:
    frame_bytes = build_frame_for_test()
    frame = parse_frame(frame_bytes)
    assert frame is not None, "parser rejected a well-formed frame"
    assert frame.receiver == 0
    assert frame.sample_rate == 192000
    assert frame.seq == 42
    assert frame.low_hz == 14_000_000
    assert frame.high_hz == 14_192_000
    assert abs(frame.floor_db - (-120.0)) < 1e-6
    assert abs(frame.scale_db - SCALE_DB) < 1e-6
    assert frame.full_span and not frame.clipped
    assert frame.nbins == 5
    assert list(frame.bins) == [0, 1, 2, 255, 128]
    assert abs(frame.dbm(0) - (-120.0)) < 1e-6
    assert abs(frame.dbm(3) - (-120.0 + 255 * SCALE_DB)) < 1e-6  # q=255: saturation

    # a non-spectrum header (audio type=1) must be ignored, not misparsed
    other = bytearray(frame_bytes)
    _HEADER_STRUCT.pack_into(other, 0, 0, 48000, 3, 0, 0, 0, 1, 2)
    assert parse_frame(bytes(other)) is None

    # truncated data must not raise
    assert parse_frame(frame_bytes[:10]) is None

    print("self-test OK: frame parser matches KTD1 layout "
          "(header 64 B + prefix 32 B + bins)")
    return 0


# --------------------------------------------------------------------------
# TCI text protocol helpers
# --------------------------------------------------------------------------

async def report_extensions(ws) -> None:
    negotiated = None
    try:
        headers = ws.response.headers
        value = headers.get("Sec-WebSocket-Extensions", "")
        negotiated = "permessage-deflate" in value
    except Exception:
        pass
    if negotiated is None:
        print("# permessage-deflate: could not be determined from this websockets version")
    else:
        print(f"# permessage-deflate: {'negotiated' if negotiated else 'not negotiated'}")


async def send_cmd(ws, text: str) -> None:
    print(f"> {text}")
    await ws.send(text)


async def recv_until(ws, prefix: str, timeout: float = 3.0):
    """Print every message as it arrives; return the first text message
    whose command name matches "prefix" (before the first ':' or ';')."""
    deadline = time.monotonic() + timeout
    while True:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            print(f"# timed out waiting for {prefix}")
            return None
        try:
            msg = await asyncio.wait_for(ws.recv(), timeout=remaining)
        except asyncio.TimeoutError:
            print(f"# timed out waiting for {prefix}")
            return None
        if isinstance(msg, (bytes, bytearray)):
            frame = parse_frame(msg)
            if frame is not None:
                print(f"< [binary spectrum frame, {len(msg)} bytes, rx={frame.receiver}]")
            continue
        for part in msg.split(";"):
            part = part.strip()
            if part:
                print(f"< {part};")
        name = msg.split(":", 1)[0].split(";", 1)[0].strip().lower()
        if name == prefix.lower():
            return msg


# --------------------------------------------------------------------------
# Subcommands
# --------------------------------------------------------------------------

async def cmd_watch(ws, args) -> int:
    await report_extensions(ws)
    await send_cmd(ws, f"spectrum_start:{args.rx},{args.bins},{args.fps};")
    if args.span is not None:
        low, high = args.span
        await send_cmd(ws, f"spectrum_span:{args.rx},{low},{high};")

    stop = asyncio.Event()
    stats = {"frames": 0, "bytes": 0, "last_seq": None, "gaps": 0,
             "low_hz": None, "high_hz": None, "floor_db": None,
             "nbins": None, "flags": None}
    first_frame_dumped = not args.dump_first

    async def reader():
        nonlocal first_frame_dumped
        while not stop.is_set():
            try:
                msg = await ws.recv()
            except websockets.exceptions.ConnectionClosed:
                stop.set()
                return
            if isinstance(msg, (bytes, bytearray)):
                frame = parse_frame(msg)
                if frame is None:
                    continue  # not type=4: audio/IQ frame, ignore
                stats["frames"] += 1
                stats["bytes"] += len(msg)
                if stats["last_seq"] is not None:
                    expected = (stats["last_seq"] + 1) & 0xFFFFFFFF
                    if frame.seq != expected:
                        gap = (frame.seq - expected) & 0xFFFFFFFF
                        stats["gaps"] += gap
                stats["last_seq"] = frame.seq
                stats["low_hz"] = frame.low_hz
                stats["high_hz"] = frame.high_hz
                stats["floor_db"] = frame.floor_db
                stats["nbins"] = frame.nbins
                stats["flags"] = frame.flags
                if not first_frame_dumped:
                    first_frame_dumped = True
                    dump_first_frame(frame)
            else:
                for part in msg.split(";"):
                    part = part.strip()
                    if part:
                        print(f"< {part};")

    async def ticker():
        while not stop.is_set():
            await asyncio.sleep(1.0)
            frames = stats["frames"]
            kbit_s = stats["bytes"] * 8 / 1000.0
            print(f"# {frames} frame/s, {kbit_s:.1f} kbit/s, seq={stats['last_seq']}, "
                  f"gaps={stats['gaps']}, span={stats['low_hz']}..{stats['high_hz']} Hz, "
                  f"floor={stats['floor_db']} dB, bins={stats['nbins']}, flags={stats['flags']}")
            stats["frames"] = 0
            stats["bytes"] = 0

    reader_task = asyncio.create_task(reader())
    ticker_task = asyncio.create_task(ticker())
    try:
        if args.seconds is not None:
            await asyncio.wait_for(asyncio.shield(reader_task), timeout=args.seconds)
    except asyncio.TimeoutError:
        pass
    except KeyboardInterrupt:
        pass
    finally:
        stop.set()
        ticker_task.cancel()
        reader_task.cancel()
        await send_cmd(ws, f"spectrum_stop:{args.rx};")
    return 0


def dump_first_frame(frame: SpectrumFrame) -> None:
    print("# first frame:")
    print(f"#   receiver={frame.receiver} sample_rate={frame.sample_rate} seq={frame.seq}")
    print(f"#   low_hz={frame.low_hz} high_hz={frame.high_hz}")
    print(f"#   floor_db={frame.floor_db} scale_db={frame.scale_db} "
          f"clipped={frame.clipped} full_span={frame.full_span}")
    print(f"#   nbins={frame.nbins}")
    head = min(16, frame.nbins)
    dbms = [f"{frame.dbm(i):.1f}" for i in range(head)]
    print(f"#   first {head} bins (dBm): {', '.join(dbms)}")


async def cmd_passive(ws, args) -> int:
    """AE1: a client that never negotiates the extension must see nothing
    spectrum-related, ever, no matter what else it asks for."""
    await send_cmd(ws, "vfo:0,0;")
    deadline = time.monotonic() + args.seconds
    violation = False
    while time.monotonic() < deadline:
        remaining = deadline - time.monotonic()
        try:
            msg = await asyncio.wait_for(ws.recv(), timeout=max(remaining, 0.01))
        except asyncio.TimeoutError:
            break
        except websockets.exceptions.ConnectionClosed:
            break
        if isinstance(msg, (bytes, bytearray)):
            frame = parse_frame(msg)
            if frame is not None:
                print(f"! unexpected type=4 frame while passive: {len(msg)} bytes")
                violation = True
            continue
        for part in msg.split(";"):
            part = part.strip()
            if not part:
                continue
            print(f"< {part};")
            if part.lower().split(":", 1)[0].startswith("spectrum_"):
                print(f"! unexpected spectrum_* message while passive: {part};")
                violation = True
    if violation:
        print("FAIL: AE1 violated, a passive client received spectrum data")
        return 1
    print(f"OK: no spectrum_* frame or message in {args.seconds:.0f} s (AE1)")
    return 0


async def cmd_att(ws, args) -> int:
    if args.value is None:
        await send_cmd(ws, f"rx_att_ex:{args.rx};")
    else:
        await send_cmd(ws, f"rx_att_ex:{args.rx},{args.value};")
    reply = await recv_until(ws, "rx_att_ex")
    if reply is None:
        return 1
    fields = reply.rstrip(";").split(":", 1)[1].split(",")
    if len(fields) >= 7:
        rx, kind, value, lo, hi, step, adc = fields[:7]
        print(f"# rx={rx} kind={kind} value={value} range=[{lo}..{hi}] step={step} adc={adc}")
    return 0


async def cmd_band(ws, args) -> int:
    if args.title is None:
        await send_cmd(ws, f"band_ex:{args.rx};")
    elif args.next:
        await send_cmd(ws, f"band_ex:{args.rx},{args.title},next;")
    else:
        await send_cmd(ws, f"band_ex:{args.rx},{args.title};")
    reply = await recv_until(ws, "band_ex")
    if reply is None:
        return 1
    if reply.rstrip(";").endswith("error"):
        print("FAIL: band_ex reported error")
        return 1
    return 0


async def cmd_span(ws, args) -> int:
    await send_cmd(ws, f"spectrum_span:{args.rx},{args.low},{args.high};")
    reply = await recv_until(ws, "spectrum_span")
    if reply is None:
        return 1
    if reply.rstrip(";").endswith("error"):
        print("FAIL: spectrum_span reported error")
        return 1
    return 0


# --------------------------------------------------------------------------
# CLI
# --------------------------------------------------------------------------

def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="tci_spectrum_probe.py",
        description="Bench probe for the deskHPSDR TCI remote extensions "
                     "(spectrum stream, rx_att_ex, band_ex).",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    parser.add_argument("--host", default="127.0.0.1", help="TCI server host (default: 127.0.0.1)")
    parser.add_argument("--port", type=int, default=40001, help="TCI server port (default: 40001)")
    parser.add_argument("--rx", type=int, default=0,
                         help="receiver index for spectrum_*/rx_att_ex, VFO index for band_ex "
                              "(default: 0)")
    parser.add_argument("--selftest", action="store_true",
                         help="run the frame-parser self-test against known bytes and exit; "
                              "no connection is made")

    sub = parser.add_subparsers(dest="command")

    watch = sub.add_parser("watch", help="subscribe and print live spectrum statistics")
    watch.add_argument("--bins", type=int, default=512, help="requested bin count (default: 512)")
    watch.add_argument("--fps", type=int, default=10, help="requested fps (default: 10)")
    watch.add_argument("--span", nargs=2, type=int, metavar=("LOW", "HIGH"),
                        help="crop to this Hz span right after subscribing")
    watch.add_argument("--seconds", type=float, default=None,
                        help="stop after this many seconds (default: run until Ctrl-C)")
    watch.add_argument("--dump-first", action="store_true",
                        help="print the first frame's header fields and first 16 bins as dBm")

    passive = sub.add_parser("passive", help="AE1 check: assert no spectrum data without a subscription")
    passive.add_argument("--seconds", type=float, default=30, help="observation window (default: 30)")

    att = sub.add_parser("att", help="read or set rx_att_ex (attenuation/gain of the receiver's ADC)")
    att.add_argument("value", nargs="?", type=int, help="new value; omit to query")

    band = sub.add_parser("band", help="read or set band_ex (band-stack change on a VFO)")
    band.add_argument("title", nargs="?", help="band title, e.g. 20, 40, 160, GEN; omit to query")
    band.add_argument("--next", action="store_true", help="advance the band-stack instead of a no-op")

    span = sub.add_parser("span", help="read or set spectrum_span without subscribing")
    span.add_argument("low", type=int, help="low edge in Hz, or 0 with high=0 for full span")
    span.add_argument("high", type=int, help="high edge in Hz")

    return parser


async def run(args) -> int:
    uri = f"ws://{args.host}:{args.port}/"
    async with websockets.connect(uri) as ws:
        if args.command == "watch":
            return await cmd_watch(ws, args)
        if args.command == "passive":
            return await cmd_passive(ws, args)
        if args.command == "att":
            return await cmd_att(ws, args)
        if args.command == "band":
            return await cmd_band(ws, args)
        if args.command == "span":
            return await cmd_span(ws, args)
    return 1


def main(argv=None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)

    if args.selftest:
        return selftest()

    if not args.command:
        parser.print_help()
        return 1

    try:
        return asyncio.run(run(args))
    except (ConnectionRefusedError, OSError) as exc:
        print(f"FAIL: could not connect to {args.host}:{args.port}: {exc}")
        return 1
    except KeyboardInterrupt:
        return 130


if __name__ == "__main__":
    sys.exit(main())

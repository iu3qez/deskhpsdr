/* Copyright (C)
* 2024-2026 - Heiko Amft, DL1BZ (Project deskHPSDR)
*
*   Binary contract and pure helpers of the TCI spectrum stream (type 4).
*   This header must stay free of GLib, GTK, WDSP and libwebsockets so the
*   module can be built and unit-tested with the plain C compiler alone.
*
* SPDX-License-Identifier: GPL-3.0-or-later
*
*   This program is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 3 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program.  If not, see <https://www.gnu.org/licenses/>.
*
*/

#ifndef _TCI_SPECTRUM_H
#define _TCI_SPECTRUM_H

#include <stdint.h>
#include <stddef.h>

//
// Wire contract of the spectrum stream.
//
// A frame is: 64 byte TCI stream header, then a fixed 32 byte payload prefix,
// then "length" bins of one byte each. Everything is little-endian.
// The client rebuilds a bin as: dbm = floor_db + q * scale_db, with q == 255
// meaning saturation.
//
// The header fields continue the TCI_STREAM_* series of src/tci_audio.h; the
// two values below are new, hence the separate header (tci_audio.h pulls in
// glib and cannot be included by the test harness).
//

#define TCI_STREAM_SPECTRUM 4     // header "type": spectrum stream
#define TCI_SPECTRUM_FORMAT_U8 4  // header "format": quantized uint8 bins

#define TCI_SPECTRUM_VERSION 1    // prefix "version"

// prefix "flags" bits
#define TCI_SPECTRUM_FLAG_CLIPPED   0x0001u  // requested span clipped to the available one
#define TCI_SPECTRUM_FLAG_FULL_SPAN 0x0002u  // the frame carries the whole available span

// Negotiable number of bins, clamped to this range before any decimation.
#define TCI_SPECTRUM_MIN_BINS 16
#define TCI_SPECTRUM_MAX_BINS 4096
#define TCI_SPECTRUM_DEFAULT_BINS 512

// Quantization step in dB, also written into every frame as scale_db.
#define TCI_SPECTRUM_SCALE_DB 0.5f

// On-the-wire sizes.
#define TCI_SPECTRUM_HEADER_BYTES 64
#define TCI_SPECTRUM_PREFIX_BYTES 32
#define TCI_SPECTRUM_FRAME_MAX_BYTES \
  (TCI_SPECTRUM_HEADER_BYTES + TCI_SPECTRUM_PREFIX_BYTES + TCI_SPECTRUM_MAX_BINS)

//
// The 32 byte payload prefix. This struct is never written to the socket as
// raw memory: tci_spectrum_serialize() emits every field explicitly, so
// padding and host endianness are irrelevant.
//
typedef struct _tci_spectrum_prefix {
  uint16_t version;   // TCI_SPECTRUM_VERSION
  uint16_t flags;     // TCI_SPECTRUM_FLAG_*
  uint32_t seq;       // per client and receiver, restarts at 0 on spectrum_start
  int64_t low_hz;     // frequency of the left edge of bin 0
  int64_t high_hz;    // frequency of the right edge of the last bin
  float floor_db;     // frame minimum, rounded down to a 0.5 dB step
  float scale_db;     // TCI_SPECTRUM_SCALE_DB
} TCI_SPECTRUM_PREFIX;

//
// Result of intersecting a requested span with the available one.
// [i0, i1) is a half-open range of pixel indices; low_hz/high_hz are the
// edges actually covered by that range.
//
typedef struct _tci_spectrum_span {
  size_t i0;          // first selected pixel
  size_t i1;          // one past the last selected pixel
  int64_t low_hz;     // effective left edge
  int64_t high_hz;    // effective right edge
  int clipped;        // 1 if the request had to be clipped
  uint16_t flags;     // TCI_SPECTRUM_FLAG_* ready for the prefix
} TCI_SPECTRUM_SPAN;

//
// Intersect the requested span with the available one.
// The available span is described by its edges, the width of one pixel in Hz
// and the pixel count. A request of 0,0 means "the whole available span".
// Returns 1 when at least one pixel is selected and *span is filled in,
// 0 when the request is empty, inverted or disjoint: the caller then emits
// no frame at all.
//
int tci_spectrum_select_span(int64_t avail_low_hz, int64_t avail_high_hz,
                             double hz_per_pixel, size_t pixels,
                             int64_t req_low_hz, int64_t req_high_hz,
                             TCI_SPECTRUM_SPAN *span);

//
// Clamp a client-requested bin count into [TCI_SPECTRUM_MIN_BINS,
// TCI_SPECTRUM_MAX_BINS]. Negative and zero requests come back as the minimum,
// so no caller can end up dividing by zero.
//
size_t tci_spectrum_clamp_bins(int bins);

//
// Max-of-N decimation, never an average: the pixels in [i0, i1) are split into
// K = min(bins, i1 - i0) contiguous groups with integer boundaries
// i0 + g * (i1 - i0) / K, and out[g] receives the maximum of group g.
// "out" must have room for K floats. Returns K, or 0 on an empty or invalid
// range.
//
size_t tci_spectrum_decimate(const float *pixels, size_t i0, size_t i1,
                             size_t bins, float *out);

//
// Frame floor: the minimum of "values", rounded DOWN to a 0.5 dB step.
// Returns 0.0f for an empty or missing input.
//
float tci_spectrum_floor_db(const float *values, size_t count);

//
// Quantize to one byte per bin: q = clamp(round((v - floor) / 0.5), 0, 255).
//
void tci_spectrum_quantize(const float *values, size_t count, float floor_db,
                           uint8_t *out);

//
// Total length on the wire of a frame carrying "nbins" bins.
//
size_t tci_spectrum_frame_bytes(size_t nbins);

//
// Serialize a complete frame into "buf": the 64 byte TCI header written by
// known offsets, the 32 byte prefix, then the bins. Returns the total length,
// or 0 if an argument is invalid or the buffer is too small.
//
size_t tci_spectrum_serialize(unsigned char *buf, size_t buf_size,
                              uint32_t receiver, uint32_t sample_rate,
                              const TCI_SPECTRUM_PREFIX *prefix,
                              const uint8_t *bins, size_t nbins);

#endif

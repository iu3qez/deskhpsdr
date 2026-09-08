/* Copyright (C)
* 2024-2026 - Heiko Amft, DL1BZ (Project deskHPSDR)
*
*   Binary contract and pure helpers of the TCI spectrum stream (type 4):
*   span intersection, max-of-N decimation, 0.5 dB quantization and frame
*   serialization. Deliberately free of GLib, GTK, WDSP and libwebsockets so
*   the module can be exercised by a standalone test harness.
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

#include <string.h>

#include "tci_spectrum.h"

//
// floor() and ceil() for the bounded magnitudes handled here (pixel indices,
// dB values): a truncating cast plus one correction step. Done by hand so the
// module needs no libm, and the test target stays a bare compiler invocation.
//
static double tci_spectrum_floor_d(double v) {
  double t = (double)(int64_t)v;

  if (t > v) { t -= 1.0; }

  return t;
}

static double tci_spectrum_ceil_d(double v) {
  double t = (double)(int64_t)v;

  if (t < v) { t += 1.0; }

  return t;
}

// Little-endian writers. Every field goes out byte by byte, so neither struct
// padding nor host endianness can leak into the wire format (the code also
// runs on ARM).
static void tci_spectrum_put_u16(unsigned char *p, uint16_t v) {
  p[0] = (unsigned char)(v & 0xFFu);
  p[1] = (unsigned char)((v >> 8) & 0xFFu);
}

static void tci_spectrum_put_u32(unsigned char *p, uint32_t v) {
  p[0] = (unsigned char)(v & 0xFFu);
  p[1] = (unsigned char)((v >> 8) & 0xFFu);
  p[2] = (unsigned char)((v >> 16) & 0xFFu);
  p[3] = (unsigned char)((v >> 24) & 0xFFu);
}

static void tci_spectrum_put_u64(unsigned char *p, uint64_t v) {
  int i;

  for (i = 0; i < 8; i++) {
    p[i] = (unsigned char)((v >> (8 * i)) & 0xFFu);
  }
}

// IEEE754 binary32, little-endian.
static void tci_spectrum_put_f32(unsigned char *p, float v) {
  uint32_t bits;
  memcpy(&bits, &v, sizeof(bits));
  tci_spectrum_put_u32(p, bits);
}

int tci_spectrum_select_span(int64_t avail_low_hz, int64_t avail_high_hz,
                             double hz_per_pixel, size_t pixels,
                             int64_t req_low_hz, int64_t req_high_hz,
                             TCI_SPECTRUM_SPAN *span) {
  int64_t lo, hi;
  size_t i0, i1;
  int clipped = 0;

  if (span == NULL) { return 0; }

  memset(span, 0, sizeof(*span));

  if (pixels == 0 || hz_per_pixel <= 0.0 || avail_high_hz <= avail_low_hz) { return 0; }

  // 0,0 is the client asking for everything the receiver has
  if (req_low_hz == 0 && req_high_hz == 0) {
    span->i0 = 0;
    span->i1 = pixels;
    span->low_hz = avail_low_hz;
    span->high_hz = avail_high_hz;
    span->clipped = 0;
    span->flags = TCI_SPECTRUM_FLAG_FULL_SPAN;
    return 1;
  }

  // empty, inverted or disjoint request: the caller emits no frame
  if (req_high_hz <= req_low_hz) { return 0; }

  if (req_high_hz <= avail_low_hz || req_low_hz >= avail_high_hz) { return 0; }

  lo = req_low_hz;
  hi = req_high_hz;

  if (lo < avail_low_hz) { lo = avail_low_hz; clipped = 1; }

  if (hi > avail_high_hz) { hi = avail_high_hz; clipped = 1; }

  // outward rounding, so the emitted span always covers what was asked for
  i0 = (size_t)tci_spectrum_floor_d((double)(lo - avail_low_hz) / hz_per_pixel);
  i1 = (size_t)tci_spectrum_ceil_d((double)(hi - avail_low_hz) / hz_per_pixel);

  if (i0 >= pixels) { i0 = pixels - 1; }

  if (i1 > pixels) { i1 = pixels; }

  if (i1 <= i0) { i1 = i0 + 1; }

  span->i0 = i0;
  span->i1 = i1;
  // the outer edges are taken verbatim, so no rounding drift shows up there
  span->low_hz = (i0 == 0) ? avail_low_hz
                 : avail_low_hz + (int64_t)((double)i0 * hz_per_pixel + 0.5);
  span->high_hz = (i1 == pixels) ? avail_high_hz
                  : avail_low_hz + (int64_t)((double)i1 * hz_per_pixel + 0.5);
  span->clipped = clipped;
  span->flags = 0;

  if (clipped) { span->flags |= TCI_SPECTRUM_FLAG_CLIPPED; }

  if (i0 == 0 && i1 == pixels) { span->flags |= TCI_SPECTRUM_FLAG_FULL_SPAN; }

  return 1;
}

size_t tci_spectrum_clamp_bins(int bins) {
  if (bins < TCI_SPECTRUM_MIN_BINS) { return (size_t)TCI_SPECTRUM_MIN_BINS; }

  if (bins > TCI_SPECTRUM_MAX_BINS) { return (size_t)TCI_SPECTRUM_MAX_BINS; }

  return (size_t)bins;
}

size_t tci_spectrum_decimate(const float *pixels, size_t i0, size_t i1,
                             size_t bins, float *out) {
  size_t n, k, g;

  if (pixels == NULL || out == NULL || bins == 0 || i1 <= i0) { return 0; }

  n = i1 - i0;
  k = (bins < n) ? bins : n;   // never interpolate: at most one bin per pixel

  for (g = 0; g < k; g++) {
    // contiguous groups with integer boundaries: no gap, no overlap
    size_t b0 = i0 + (g * n) / k;
    size_t b1 = i0 + ((g + 1) * n) / k;
    size_t i;
    float m;

    if (b1 <= b0) { b1 = b0 + 1; }

    m = pixels[b0];

    for (i = b0 + 1; i < b1; i++) {
      if (pixels[i] > m) { m = pixels[i]; }   // max of N, never an average
    }

    out[g] = m;
  }

  return k;
}

float tci_spectrum_floor_db(const float *values, size_t count) {
  size_t i;
  float min;

  if (values == NULL || count == 0) { return 0.0f; }

  min = values[0];

  for (i = 1; i < count; i++) {
    if (values[i] < min) { min = values[i]; }
  }

  // round the minimum DOWN to a whole 0.5 dB step
  return (float)(tci_spectrum_floor_d((double)min / (double)TCI_SPECTRUM_SCALE_DB)
                 * (double)TCI_SPECTRUM_SCALE_DB);
}

void tci_spectrum_quantize(const float *values, size_t count, float floor_db,
                           uint8_t *out) {
  size_t i;

  if (values == NULL || out == NULL) { return; }

  for (i = 0; i < count; i++) {
    double q = ((double)values[i] - (double)floor_db) / (double)TCI_SPECTRUM_SCALE_DB;

    if (q <= 0.0) {              // at or below the floor
      out[i] = 0;
      continue;
    }

    q += 0.5;                    // q is positive here, so this is round()

    if (q >= 255.0) {            // 255 means saturation
      out[i] = 255;
      continue;
    }

    out[i] = (uint8_t)q;
  }
}

size_t tci_spectrum_frame_bytes(size_t nbins) {
  return (size_t)TCI_SPECTRUM_HEADER_BYTES + (size_t)TCI_SPECTRUM_PREFIX_BYTES + nbins;
}

size_t tci_spectrum_serialize(unsigned char *buf, size_t buf_size,
                              uint32_t receiver, uint32_t sample_rate,
                              const TCI_SPECTRUM_PREFIX *prefix,
                              const uint8_t *bins, size_t nbins) {
  unsigned char *p;
  size_t total;

  if (buf == NULL || prefix == NULL || bins == NULL || nbins == 0) { return 0; }

  total = tci_spectrum_frame_bytes(nbins);

  if (buf_size < total) { return 0; }

  // 64 byte TCI stream header, written by offset: the TCI_STREAM_HEADER
  // typedef lives in tci_audio.h, which pulls in glib and must stay out here.
  memset(buf, 0, (size_t)TCI_SPECTRUM_HEADER_BYTES);
  tci_spectrum_put_u32(buf +  0, receiver);
  tci_spectrum_put_u32(buf +  4, sample_rate);
  tci_spectrum_put_u32(buf +  8, (uint32_t)TCI_SPECTRUM_FORMAT_U8);
  tci_spectrum_put_u32(buf + 12, 0u);                    // codec
  tci_spectrum_put_u32(buf + 16, 0u);                    // crc
  tci_spectrum_put_u32(buf + 20, (uint32_t)nbins);       // length = number of bins
  tci_spectrum_put_u32(buf + 24, (uint32_t)TCI_STREAM_SPECTRUM);
  tci_spectrum_put_u32(buf + 28, 1u);                    // channels
  // reserv[8] at offset 32..63 stays zero from the memset above
  // 32 byte payload prefix
  p = buf + TCI_SPECTRUM_HEADER_BYTES;
  tci_spectrum_put_u16(p +  0, prefix->version);
  tci_spectrum_put_u16(p +  2, prefix->flags);
  tci_spectrum_put_u32(p +  4, prefix->seq);
  tci_spectrum_put_u64(p +  8, (uint64_t)prefix->low_hz);
  tci_spectrum_put_u64(p + 16, (uint64_t)prefix->high_hz);
  tci_spectrum_put_f32(p + 24, prefix->floor_db);
  tci_spectrum_put_f32(p + 28, prefix->scale_db);
  // bins
  memcpy(buf + TCI_SPECTRUM_HEADER_BYTES + TCI_SPECTRUM_PREFIX_BYTES, bins, nbins);
  return total;
}

//
// Adaptive fps ladder (R4, KTD5). Request level rungs, highest first.
//
static const int tci_spectrum_ladder_rungs[TCI_SPECTRUM_LADDER_LEVELS] = { 20, 10, 5 };

void tci_spectrum_ladder_init(TCI_SPECTRUM_LADDER *l, int64_t now_us) {
  if (l == NULL) { return; }

  l->level = 0;
  l->replaced_at_window_start = 0;
  l->last_replaced = 0;
  l->window_start_us = now_us;
  l->last_replaced_us = now_us;
}

int tci_spectrum_ladder_update(TCI_SPECTRUM_LADDER *l, uint32_t replaced_total,
                               int64_t now_us) {
  uint32_t previous;

  if (l == NULL) { return 0; }

  //
  // spectrum_start zeroes the client counter without going through init():
  // resynchronise instead of reading the step backwards as a burst.
  //
  if (replaced_total < l->last_replaced) {
    l->last_replaced = replaced_total;
    l->replaced_at_window_start = replaced_total;
    l->window_start_us = now_us;
    l->last_replaced_us = now_us;
    return l->level;
  }

  previous = l->last_replaced;

  if (replaced_total != previous) {
    l->last_replaced = replaced_total;
    l->last_replaced_us = now_us;      // the quiet timer restarts from here
  }

  //
  // Roll an expired window BEFORE counting, so replacements spread over more
  // than one second never add up: what came in on this very tick belongs to
  // the new window, hence the base is the count seen at the previous tick.
  //
  if (now_us - l->window_start_us >= (int64_t) TCI_SPECTRUM_LADDER_WINDOW_US) {
    l->window_start_us = now_us;
    l->replaced_at_window_start = previous;
  }

  if (replaced_total - l->replaced_at_window_start >= (uint32_t) TCI_SPECTRUM_LADDER_TRIGGER) {
    // the socket is saturating: one rung down, and a fresh window
    if (l->level < TCI_SPECTRUM_LADDER_LEVELS - 1) { l->level++; }

    l->window_start_us = now_us;
    l->replaced_at_window_start = replaced_total;
  } else if (l->level > 0 &&
             now_us - l->last_replaced_us >= (int64_t) TCI_SPECTRUM_LADDER_CLIMB_US) {
    // ten seconds without a single replacement: one rung back up
    l->level--;
    l->last_replaced_us = now_us;      // the next climb needs another ten seconds
  }

  return l->level;
}

int tci_spectrum_ladder_fps(int level, int requested) {
  int base;
  int idx;

  if (requested < 1) { requested = 1; }

  if (level < 0) { level = 0; }

  if (level > TCI_SPECTRUM_LADDER_LEVELS - 1) { level = TCI_SPECTRUM_LADDER_LEVELS - 1; }

  // a request below the last rung has no ladder: it is already as low as it goes
  if (requested < tci_spectrum_ladder_rungs[TCI_SPECTRUM_LADDER_LEVELS - 1]) { return requested; }

  // start from the highest rung not above the request
  base = 0;

  while (base < TCI_SPECTRUM_LADDER_LEVELS - 1 &&
         tci_spectrum_ladder_rungs[base] > requested) {
    base++;
  }

  idx = base + level;

  if (idx > TCI_SPECTRUM_LADDER_LEVELS - 1) { idx = TCI_SPECTRUM_LADDER_LEVELS - 1; }

  return tci_spectrum_ladder_rungs[idx];
}

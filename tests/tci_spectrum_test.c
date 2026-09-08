/* Copyright (C)
* 2024-2026 - Heiko Amft, DL1BZ (Project deskHPSDR)
*
*   Standalone harness for the pure TCI spectrum module (src/tci_spectrum.c).
*   Built with the plain C compiler, without GTK, GLib, WDSP or libwebsockets.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tci_spectrum.h"

static int tests_run = 0;
static int tests_failed = 0;

#define CHECK(cond) do { \
    tests_run++; \
    if (!(cond)) { \
      tests_failed++; \
      fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
    } \
  } while (0)

/* Available span used by the intersection tests: 1000 pixels, 100 Hz each. */
#define AVAIL_LOW  ((int64_t)14000000)
#define AVAIL_HIGH ((int64_t)14100000)
#define AVAIL_HZPP (100.0)
#define AVAIL_PIX  ((size_t)1000)

static int select_span(int64_t req_low, int64_t req_high, TCI_SPECTRUM_SPAN *span) {
  return tci_spectrum_select_span(AVAIL_LOW, AVAIL_HIGH, AVAIL_HZPP, AVAIL_PIX,
                                  req_low, req_high, span);
}

/* Group boundary as documented in KTD1/U1: i0 + g * (i1 - i0) / K, integer math. */
static size_t group_bound(size_t i0, size_t n, size_t k, size_t g) {
  return i0 + (g * n) / k;
}

static void test_span_inside(void) {
  TCI_SPECTRUM_SPAN s;
  memset(&s, 0, sizeof(s));
  CHECK(select_span(14020000, 14050000, &s) == 1);
  CHECK(s.i0 == 200);
  CHECK(s.i1 == 500);
  CHECK(s.clipped == 0);
  CHECK(s.low_hz == 14020000);
  CHECK(s.high_hz == 14050000);
  CHECK((s.flags & TCI_SPECTRUM_FLAG_CLIPPED) == 0);
  CHECK((s.flags & TCI_SPECTRUM_FLAG_FULL_SPAN) == 0);
}

static void test_span_full(void) {
  TCI_SPECTRUM_SPAN s;
  memset(&s, 0, sizeof(s));
  /* 0,0 means "give me the whole available span" */
  CHECK(select_span(0, 0, &s) == 1);
  CHECK(s.i0 == 0);
  CHECK(s.i1 == AVAIL_PIX);
  CHECK(s.clipped == 0);
  CHECK(s.low_hz == AVAIL_LOW);
  CHECK(s.high_hz == AVAIL_HIGH);
  CHECK((s.flags & TCI_SPECTRUM_FLAG_FULL_SPAN) != 0);
  CHECK((s.flags & TCI_SPECTRUM_FLAG_CLIPPED) == 0);
}

static void test_span_overflow(void) {
  TCI_SPECTRUM_SPAN s;
  memset(&s, 0, sizeof(s));
  /* overflows on both sides -> clamped to the available span, clipped flag set */
  CHECK(select_span(13900000, 14200000, &s) == 1);
  CHECK(s.i0 == 0);
  CHECK(s.i1 == AVAIL_PIX);
  CHECK(s.clipped == 1);
  CHECK(s.low_hz == AVAIL_LOW);
  CHECK(s.high_hz == AVAIL_HIGH);
  CHECK((s.flags & TCI_SPECTRUM_FLAG_CLIPPED) != 0);
  CHECK((s.flags & TCI_SPECTRUM_FLAG_FULL_SPAN) != 0);
  /* overflow on the left only */
  memset(&s, 0, sizeof(s));
  CHECK(select_span(13900000, 14030000, &s) == 1);
  CHECK(s.i0 == 0);
  CHECK(s.i1 == 300);
  CHECK(s.clipped == 1);
  CHECK(s.low_hz == AVAIL_LOW);
  CHECK(s.high_hz == 14030000);
  CHECK((s.flags & TCI_SPECTRUM_FLAG_FULL_SPAN) == 0);
  /* overflow on the right only */
  memset(&s, 0, sizeof(s));
  CHECK(select_span(14070000, 14200000, &s) == 1);
  CHECK(s.i0 == 700);
  CHECK(s.i1 == AVAIL_PIX);
  CHECK(s.clipped == 1);
  CHECK(s.low_hz == 14070000);
  CHECK(s.high_hz == AVAIL_HIGH);
}

static void test_span_disjoint(void) {
  TCI_SPECTRUM_SPAN s;
  /* entirely above */
  memset(&s, 0, sizeof(s));
  CHECK(select_span(14200000, 14300000, &s) == 0);
  CHECK(s.i0 == s.i1);
  /* entirely below */
  memset(&s, 0, sizeof(s));
  CHECK(select_span(13800000, 13900000, &s) == 0);
  CHECK(s.i0 == s.i1);
  /* touching an edge is still empty: the span is half-open */
  memset(&s, 0, sizeof(s));
  CHECK(select_span(14100000, 14200000, &s) == 0);
  memset(&s, 0, sizeof(s));
  CHECK(select_span(13900000, 14000000, &s) == 0);
  /* degenerate / inverted request */
  memset(&s, 0, sizeof(s));
  CHECK(select_span(14050000, 14020000, &s) == 0);
}

static void test_bins_clamp(void) {
  CHECK(tci_spectrum_clamp_bins(0) == TCI_SPECTRUM_MIN_BINS);
  CHECK(tci_spectrum_clamp_bins(-5) == TCI_SPECTRUM_MIN_BINS);
  CHECK(tci_spectrum_clamp_bins(15) == TCI_SPECTRUM_MIN_BINS);
  CHECK(tci_spectrum_clamp_bins(16) == 16);
  CHECK(tci_spectrum_clamp_bins(512) == 512);
  CHECK(tci_spectrum_clamp_bins(4096) == 4096);
  CHECK(tci_spectrum_clamp_bins(100000) == TCI_SPECTRUM_MAX_BINS);
  CHECK(TCI_SPECTRUM_MIN_BINS == 16);
  CHECK(TCI_SPECTRUM_MAX_BINS == 4096);
}

static void test_decimate_1000_to_512(void) {
  static float px[1000];
  static float out[TCI_SPECTRUM_MAX_BINS];
  const size_t n = 1000;
  size_t k, g, covered, i;

  /* strictly increasing: the max of a group is the value at its last pixel */
  for (i = 0; i < n; i++) { px[i] = (float)i; }

  k = tci_spectrum_decimate(px, 0, n, tci_spectrum_clamp_bins(512), out);
  CHECK(k == 512);
  covered = 0;

  for (g = 0; g < k; g++) {
    size_t b0 = group_bound(0, n, k, g);
    size_t b1 = group_bound(0, n, k, g + 1);
    CHECK(b1 > b0);                    /* no empty group */
    CHECK(b0 == covered);              /* contiguous: no gap and no overlap */
    covered = b1;
    CHECK(out[g] == (float)(b1 - 1));  /* the max of the group */
  }

  CHECK(covered == n);                 /* every pixel belongs to exactly one group */
  CHECK(group_bound(0, n, k, 0) == 0);
  /* strictly decreasing: the max of a group is the value at its first pixel;
     together with the ascending pass this pins both edges of every group */
  for (i = 0; i < n; i++) { px[i] = (float)(n - i); }

  k = tci_spectrum_decimate(px, 0, n, tci_spectrum_clamp_bins(512), out);
  CHECK(k == 512);

  for (g = 0; g < k; g++) {
    size_t b0 = group_bound(0, n, k, g);
    CHECK(out[g] == (float)(n - b0));
  }
}

static void test_decimate_more_bins_than_pixels(void) {
  static float px[1000];
  static float out[TCI_SPECTRUM_MAX_BINS];
  size_t k, i;

  for (i = 0; i < 1000; i++) { px[i] = (float)((i * 3) % 977); }

  /* 4096 bins requested on 1000 pixels -> K = 1000, no interpolation */
  k = tci_spectrum_decimate(px, 0, 1000, tci_spectrum_clamp_bins(4096), out);
  CHECK(k == 1000);

  for (i = 0; i < k; i++) { CHECK(out[i] == px[i]); }
}

static void test_decimate_subrange_and_guards(void) {
  static float px[1000];
  static float out[TCI_SPECTRUM_MAX_BINS];
  size_t k, i;

  for (i = 0; i < 1000; i++) { px[i] = (float)i; }

  /* sub-range [200,500) with 512 bins requested -> K = 300 */
  k = tci_spectrum_decimate(px, 200, 500, tci_spectrum_clamp_bins(512), out);
  CHECK(k == 300);
  CHECK(out[0] == 200.0f);
  CHECK(out[299] == 499.0f);
  /* clamped bin counts never divide by zero, not even the degenerate requests */
  k = tci_spectrum_decimate(px, 0, 1000, tci_spectrum_clamp_bins(0), out);
  CHECK(k == 16);
  k = tci_spectrum_decimate(px, 0, 1000, tci_spectrum_clamp_bins(-5), out);
  CHECK(k == 16);
  k = tci_spectrum_decimate(px, 0, 1000, tci_spectrum_clamp_bins(100000), out);
  CHECK(k == 1000);
  /* an empty or inverted range yields nothing */
  k = tci_spectrum_decimate(px, 500, 500, 512, out);
  CHECK(k == 0);
  k = tci_spectrum_decimate(px, 500, 400, 512, out);
  CHECK(k == 0);
  k = tci_spectrum_decimate(NULL, 0, 1000, 512, out);
  CHECK(k == 0);
}

static void test_floor_db(void) {
  float v[3];
  v[0] = -100.3f; v[1] = -80.0f; v[2] = -90.0f;
  CHECK(tci_spectrum_floor_db(v, 3) == -100.5f);   /* rounded DOWN to 0.5 dB */
  v[0] = -100.0f; v[1] = -80.0f; v[2] = -90.0f;
  CHECK(tci_spectrum_floor_db(v, 3) == -100.0f);   /* already on the grid */
  v[0] = -99.75f; v[1] = -80.0f; v[2] = -90.0f;
  CHECK(tci_spectrum_floor_db(v, 3) == -100.0f);
  v[0] = -100.5f; v[1] = -80.0f; v[2] = -90.0f;
  CHECK(tci_spectrum_floor_db(v, 3) == -100.5f);
  v[0] = 3.25f; v[1] = 10.0f; v[2] = 7.0f;         /* positive values too */
  CHECK(tci_spectrum_floor_db(v, 3) == 3.0f);
  CHECK(tci_spectrum_floor_db(NULL, 3) == 0.0f);
  CHECK(tci_spectrum_floor_db(v, 0) == 0.0f);
}

static void test_quantize(void) {
  float v[6];
  uint8_t q[6];
  CHECK(TCI_SPECTRUM_SCALE_DB == 0.5f);
  v[0] = -120.0f;   /* exactly the floor           -> 0   */
  v[1] = -119.8f;   /* above the floor, rounds down -> 0  */
  v[2] = -119.7f;   /* above the floor, rounds up  -> 1   */
  v[3] = 7.5f;      /* floor + 127.5 dB            -> 255 */
  v[4] = 50.0f;     /* past the top of the range   -> 255 */
  v[5] = -130.0f;   /* below the floor             -> 0   */
  memset(q, 0xAA, sizeof(q));
  tci_spectrum_quantize(v, 6, -120.0f, q);
  CHECK(q[0] == 0);
  CHECK(q[1] == 0);
  CHECK(q[2] == 1);
  CHECK(q[3] == 255);
  CHECK(q[4] == 255);
  CHECK(q[5] == 0);
  /* a floor derived from the frame keeps every bin inside [0,255] */
  {
    float w[4];
    uint8_t p[4];
    float fl;
    w[0] = -101.3f; w[1] = -101.3f; w[2] = -100.8f; w[3] = -50.0f;
    fl = tci_spectrum_floor_db(w, 4);
    CHECK(fl == -101.5f);
    tci_spectrum_quantize(w, 4, fl, p);
    CHECK(p[0] == 0);   /* 0.2 dB above the floor rounds to 0 */
    CHECK(p[1] == 0);
    CHECK(p[2] == 1);   /* 0.7 dB above the floor rounds to 1 */
    CHECK(p[3] == 103); /* 51.5 dB / 0.5 dB */
  }
}

/* Byte-exact contract of KTD1: 64 byte TCI header, 32 byte prefix, then the bins. */
static void test_serialize_bytes(void) {
  static const unsigned char expected[100] = {
    /* --- 64 byte TCI stream header, uint32 little-endian --- */
    0x01, 0x00, 0x00, 0x00,   /* receiver    = 1        */
    0x00, 0xEE, 0x02, 0x00,   /* sample_rate = 192000   */
    0x04, 0x00, 0x00, 0x00,   /* format      = 4        */
    0x00, 0x00, 0x00, 0x00,   /* codec       = 0        */
    0x00, 0x00, 0x00, 0x00,   /* crc         = 0        */
    0x04, 0x00, 0x00, 0x00,   /* length      = 4 bins   */
    0x04, 0x00, 0x00, 0x00,   /* type        = 4        */
    0x01, 0x00, 0x00, 0x00,   /* channels    = 1        */
    0x00, 0x00, 0x00, 0x00,   /* reserv[0]              */
    0x00, 0x00, 0x00, 0x00,   /* reserv[1]              */
    0x00, 0x00, 0x00, 0x00,   /* reserv[2]              */
    0x00, 0x00, 0x00, 0x00,   /* reserv[3]              */
    0x00, 0x00, 0x00, 0x00,   /* reserv[4]              */
    0x00, 0x00, 0x00, 0x00,   /* reserv[5]              */
    0x00, 0x00, 0x00, 0x00,   /* reserv[6]              */
    0x00, 0x00, 0x00, 0x00,   /* reserv[7]              */
    /* --- 32 byte payload prefix --- */
    0x01, 0x00,                                        /* version  = 1         */
    0x02, 0x00,                                        /* flags    = full span */
    0x07, 0x00, 0x00, 0x00,                            /* seq      = 7         */
    0x80, 0x9F, 0xD5, 0x00, 0x00, 0x00, 0x00, 0x00,    /* low_hz   = 14000000  */
    0x80, 0x8D, 0xD8, 0x00, 0x00, 0x00, 0x00, 0x00,    /* high_hz  = 14192000  */
    0x00, 0x00, 0xF0, 0xC2,                            /* floor_db = -120.0f   */
    0x00, 0x00, 0x00, 0x3F,                            /* scale_db = 0.5f      */
    /* --- bins --- */
    0x00, 0x01, 0xFE, 0xFF
  };
  static const uint8_t bins[4] = { 0, 1, 254, 255 };
  unsigned char buf[128];
  TCI_SPECTRUM_PREFIX pfx;
  size_t len;
  CHECK(TCI_SPECTRUM_HEADER_BYTES == 64);
  CHECK(TCI_SPECTRUM_PREFIX_BYTES == 32);
  CHECK(TCI_STREAM_SPECTRUM == 4);
  CHECK(TCI_SPECTRUM_FORMAT_U8 == 4);
  CHECK(TCI_SPECTRUM_VERSION == 1);
  CHECK(TCI_SPECTRUM_FLAG_CLIPPED == 0x0001);
  CHECK(TCI_SPECTRUM_FLAG_FULL_SPAN == 0x0002);
  memset(buf, 0x5A, sizeof(buf));
  memset(&pfx, 0, sizeof(pfx));
  pfx.version = TCI_SPECTRUM_VERSION;
  pfx.flags = TCI_SPECTRUM_FLAG_FULL_SPAN;
  pfx.seq = 7;
  pfx.low_hz = 14000000;
  pfx.high_hz = 14192000;
  pfx.floor_db = -120.0f;
  pfx.scale_db = TCI_SPECTRUM_SCALE_DB;
  len = tci_spectrum_serialize(buf, sizeof(buf), 1, 192000, &pfx, bins, 4);
  CHECK(len == 100);
  CHECK(len == tci_spectrum_frame_bytes(4));
  CHECK(memcmp(buf, expected, sizeof(expected)) == 0);

  if (memcmp(buf, expected, sizeof(expected)) != 0) {
    size_t i;

    for (i = 0; i < sizeof(expected); i++) {
      if (buf[i] != expected[i]) {
        fprintf(stderr, "  byte %zu: got 0x%02X, expected 0x%02X\n",
                i, buf[i], expected[i]);
      }
    }
  }

  /* nothing is written past the end of the frame */
  CHECK(buf[100] == 0x5A);
  /* guards */
  CHECK(tci_spectrum_serialize(buf, 99, 1, 192000, &pfx, bins, 4) == 0);
  CHECK(tci_spectrum_serialize(NULL, 128, 1, 192000, &pfx, bins, 4) == 0);
  CHECK(tci_spectrum_serialize(buf, sizeof(buf), 1, 192000, NULL, bins, 4) == 0);
  CHECK(tci_spectrum_serialize(buf, sizeof(buf), 1, 192000, &pfx, NULL, 4) == 0);
  CHECK(tci_spectrum_serialize(buf, sizeof(buf), 1, 192000, &pfx, bins, 0) == 0);
}

int main(void) {
  test_span_inside();
  test_span_full();
  test_span_overflow();
  test_span_disjoint();
  test_bins_clamp();
  test_decimate_1000_to_512();
  test_decimate_more_bins_than_pixels();
  test_decimate_subrange_and_guards();
  test_floor_db();
  test_quantize();
  test_serialize_bytes();
  printf("tci_spectrum_test: %d checks, %d failed\n", tests_run, tests_failed);
  return tests_failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

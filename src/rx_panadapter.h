/* Copyright (C)
* 2015 - John Melton, G0ORX/N6LYT
* 2024-2026 - Heiko Amft, DL1BZ (Project deskHPSDR)
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

#ifndef _PANADAPTER_H
#define _PANADAPTER_H

// int compare_doubles(const void *a, const void *b);
void panadapter_set_max_label_rows(int r);
void pan_add_label(long long freq, const char *text);
void pan_add_label_timeout(long long freq, const char *text, int lifetime_ms);
void pan_clear_labels(void);
typedef enum {
  PAN_SPOT_SOURCE_CUSTOM = 0,
  PAN_SPOT_SOURCE_CLUSTER,
  PAN_SPOT_SOURCE_TCI,
  PAN_SPOT_SOURCE_RBN
} PAN_SPOT_SOURCE;

void pan_delete_dx_spot(const char *dxcall);
void pan_add_dx_spot(double freq_khz, const char *dxcall);
void pan_add_dx_spot_source(double freq_khz, const char *dxcall, PAN_SPOT_SOURCE source);
//
// Pixel -> Hz and raw -> dBm mapping of a receiver (KTD2).
//
// low_hz is the absolute frequency of rx->pixel_samples[0], i.e. the mapping
// taken at pan = 0; the frequency of bin i is low_hz + i * hz_per_pixel.
// soffset is the dB correction the panadapter adds to every raw bin.
//
typedef struct _rx_pan_mapping {
  int vfo_id;             // VFO the panadapter shows for this receiver
  long long center_hz;    // vfo[vfo_id].frequency after the CW sidetone shift
  long long cw_shift_hz;  // pixel shift in Hz applied in CWU (+) / CWL (-)
  long long dc_offset_hz; // mode DC offset (AM/SAM), drawn as pan_display_shift
  double hz_per_pixel;    // width of one bin of rx->pixel_samples
  long long low_hz;       // absolute frequency of rx->pixel_samples[0]
  double soffset;         // dB correction: calibration, attenuation, preamps
} RX_PAN_MAPPING;

void rx_panadapter_get_mapping(const RECEIVER *rx, RX_PAN_MAPPING *map);
void rx_panadapter_peak_hold_clear(RECEIVER *rx);
void rx_panadapter_update(RECEIVER* rx);
void rx_panadapter_init(RECEIVER *rx, int width, int height);
void rx_panadapter_draw_frequency_markers(const RECEIVER *rx, cairo_t *cr, int width, double y);
void display_panadapter_messages(cairo_t *cr, int width, unsigned int fps);
void rx_update_mnf_from_gui(RECEIVER *rx);
void rx_update_mnf_run_from_gui(RECEIVER *rx);
extern void rx_panadapter_force_noisefloor_update(void);

#endif

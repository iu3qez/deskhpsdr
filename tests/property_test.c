/* Copyright (C)
* 2024-2026 - Heiko Amft, DL1BZ (Project deskHPSDR)
*
*   Standalone harness for the props file handling (src/property.c).
*   Needs GTK/GLib because property.c uses g_strdup() and GHashTable.
*
*   The interesting case is the write-back of unknown keys: an older build
*   must not destroy the settings of a newer one when it saves at exit.
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

#include <gtk/gtk.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "discovered.h"
#include "property.h"

//
// property.c reaches for the discovered radio (Saturn props file name hook)
// and for t_print(). Neither is exercised here, so plain stubs will do.
//
DISCOVERED *radio = NULL;

void t_print(const char *format, ...) {
  (void)format;
}

static int tests_run = 0;
static int tests_failed = 0;

#define CHECK(cond) do { \
    tests_run++; \
    if (!(cond)) { \
      tests_failed++; \
      fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
    } \
  } while (0)

#define CHECK_PROP(key, want) do { \
    const char *got_ = getProperty(key); \
    tests_run++; \
    if (got_ == NULL || strcmp(got_, want) != 0) { \
      tests_failed++; \
      fprintf(stderr, "FAIL %s:%d: %s is \"%s\", expected \"%s\"\n", \
              __FILE__, __LINE__, key, got_ ? got_ : "(absent)", want); \
    } \
  } while (0)

static const char *props_file = "property_test.props";

static void write_props(const char *body) {
  FILE *f = fopen(props_file, "w");
  fputs(body, f);
  fclose(f);
}

//
// A props file written by a build that knows tci_bind_addr and tci_enable,
// read back by a build that does not. The old build must rewrite what it
// knows and leave the rest untouched.
//
static void test_unknown_keys_survive_an_older_build(void) {
  write_props("property_version=3.00\n"
              "tci_bind_addr=127.0.0.1\n"
              "tci_enable=1\n"
              "rx[0].volume=0.50\n");
  loadProperties(props_file);
  CHECK(snapshotProperties() == 3);            // property_version is not snapshotted
  // What radio_save_state() does: drop everything, write back the known keys.
  clearProperties();
  setProperty("rx[0].volume", "0.80");
  CHECK(restoreUnknownProperties() == 2);
  saveProperties(props_file);
  clearProperties();
  clearPropertiesSnapshot();
  loadProperties(props_file);
  CHECK_PROP("rx[0].volume", "0.80");          // updated by the old build
  CHECK_PROP("tci_bind_addr", "127.0.0.1");    // preserved
  CHECK_PROP("tci_enable", "1");               // preserved
  CHECK_PROP("property_version", "3.00");
}

//
// A key the running build does write must keep the new value, never the
// snapshotted one.
//
static void test_known_keys_are_not_overwritten_by_the_snapshot(void) {
  write_props("property_version=3.00\n"
              "rx[0].volume=0.50\n");
  loadProperties(props_file);
  CHECK(snapshotProperties() == 1);
  clearProperties();
  setProperty("rx[0].volume", "0.10");
  CHECK(restoreUnknownProperties() == 0);
  CHECK_PROP("rx[0].volume", "0.10");
}

//
// Without a snapshot the behaviour is the old one: nothing is written back.
// This is what keeps the other props files (css, band, voicekeyer) clean,
// they never call snapshotProperties().
//
static void test_no_snapshot_means_no_write_back(void) {
  write_props("property_version=3.00\n"
              "tci_bind_addr=127.0.0.1\n");
  loadProperties(props_file);
  clearPropertiesSnapshot();
  clearProperties();
  setProperty("something.else", "1");
  CHECK(restoreUnknownProperties() == 0);
  CHECK(getProperty("tci_bind_addr") == NULL);
}

//
// A props file of a different version is discarded by loadProperties(), so
// there is nothing to snapshot and nothing to write back.
//
static void test_version_mismatch_leaves_nothing_to_restore(void) {
  write_props("property_version=2.00\n"
              "tci_bind_addr=127.0.0.1\n");
  loadProperties(props_file);
  CHECK(snapshotProperties() == 0);
  CHECK(restoreUnknownProperties() == 0);
}

int main(void) {
  test_unknown_keys_survive_an_older_build();
  test_known_keys_are_not_overwritten_by_the_snapshot();
  test_no_snapshot_means_no_write_back();
  test_version_mismatch_leaves_nothing_to_restore();
  clearProperties();
  clearPropertiesSnapshot();
  unlink(props_file);
  printf("property_test: %d checks, %d failed\n", tests_run, tests_failed);
  return tests_failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* Copyright (C)
* 2024-2026 - Heiko Amft, DL1BZ (Project deskHPSDR)
*
*   Model of the client snapshot pattern of src/tci.c, to be run under
*   AddressSanitizer.
*
*   IMPORTANT: this harness does NOT exercise src/tci.c. It reproduces the
*   shape of the pattern in isolation: a list of clients guarded by a mutex,
*   walkers that copy it and dereference the copies with the mutex released,
*   and a closer that removes a client from the list and then frees its
*   storage, the way libwebsockets frees the per-session data of a wsi once
*   LWS_CALLBACK_CLOSED has returned.
*
*   Its job is to keep the invariant honest: a walk in flight must hold off
*   that free. Built as it is, it must run clean. Built with
*   -DTCI_SNAPSHOT_MODEL_UNGUARDED=1 it drops the walker accounting and
*   AddressSanitizer reports the heap-use-after-free that the accounting
*   exists to prevent.
*
*   Reproducing the bug against the real server is a different exercise and
*   needs a client subscribing to the spectrum and reconnecting in a loop.
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

#include <glib.h>

#include <stdio.h>
#include <stdlib.h>

#ifndef TCI_SNAPSHOT_MODEL_UNGUARDED
#define TCI_SNAPSHOT_MODEL_UNGUARDED 0
#endif

#define WALKER_THREADS 4
#define CONNECT_CYCLES 3000

typedef struct {
  int running;
  int seq;
  int touched;
} CLIENT;

static GMutex tci_mutex;
static GList *tci_clients = NULL;
#if !TCI_SNAPSHOT_MODEL_UNGUARDED
static int tci_snapshot_walkers = 0;
static GCond tci_snapshot_done;
#endif
static volatile int done = 0;
static int timeouts = 0;

static GList *tci_clients_snapshot(void) {
  GList *clients;
  g_mutex_lock(&tci_mutex);
  clients = g_list_copy(tci_clients);
#if !TCI_SNAPSHOT_MODEL_UNGUARDED
  tci_snapshot_walkers++;
#endif
  g_mutex_unlock(&tci_mutex);
  return clients;
}

static void tci_clients_snapshot_free(GList *clients) {
#if !TCI_SNAPSHOT_MODEL_UNGUARDED
  g_mutex_lock(&tci_mutex);

  if (tci_snapshot_walkers > 0) {
    tci_snapshot_walkers--;
  }

  if (tci_snapshot_walkers == 0) {
    g_cond_broadcast(&tci_snapshot_done);
  }

  g_mutex_unlock(&tci_mutex);
#endif
  g_list_free(clients);
}

//
// A broadcast: walk the snapshot with the mutex released and touch each
// client, which is the dereference that can land on freed storage.
//
static gpointer walker(gpointer data) {
  (void) data;

  while (!done) {
    GList *clients = tci_clients_snapshot();

    for (GList *l = clients; l != NULL; l = l->next) {
      CLIENT *client = (CLIENT *) l->data;
      g_thread_yield();                 // widen the window, as the producer does

      if (client->running) {
        client->touched++;
      }
    }

    tci_clients_snapshot_free(clients);
  }

  return NULL;
}

//
// A client connecting and disconnecting in a loop. The free at the end stands
// for what libwebsockets does after LWS_CALLBACK_CLOSED has returned.
//
static gpointer closer(gpointer data) {
  (void) data;

  for (int i = 0; i < CONNECT_CYCLES; i++) {
    CLIENT *client = g_new0(CLIENT, 1);
    client->running = 1;
    client->seq = i;
    g_mutex_lock(&tci_mutex);
    tci_clients = g_list_append(tci_clients, client);
    g_mutex_unlock(&tci_mutex);
    g_thread_yield();
    g_mutex_lock(&tci_mutex);
    client->running = 0;
    tci_clients = g_list_remove(tci_clients, client);
#if !TCI_SNAPSHOT_MODEL_UNGUARDED
    {
      gint64 deadline = g_get_monotonic_time() + 2 * G_TIME_SPAN_SECOND;

      while (tci_snapshot_walkers > 0) {
        if (!g_cond_wait_until(&tci_snapshot_done, &tci_mutex, deadline)) {
          timeouts++;
          break;
        }
      }
    }
#endif
    g_mutex_unlock(&tci_mutex);
    g_free(client);
  }

  done = 1;
  return NULL;
}

int main(void) {
  GThread *walkers[WALKER_THREADS];
  GThread *close_thread;

  for (int i = 0; i < WALKER_THREADS; i++) {
    walkers[i] = g_thread_new("walker", walker, NULL);
  }

  close_thread = g_thread_new("closer", closer, NULL);
  g_thread_join(close_thread);

  for (int i = 0; i < WALKER_THREADS; i++) {
    g_thread_join(walkers[i]);
  }

  printf("tci_snapshot_model_test: %d cycles, %d walkers, %d wait timeouts\n",
         CONNECT_CYCLES, WALKER_THREADS, timeouts);

  if (timeouts != 0) {
    fprintf(stderr, "FAIL: a walk did not finish within the bounded wait\n");
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

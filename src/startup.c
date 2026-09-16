/* Copyright (C)
* 2023 - Christoph van Wüllen, DL1YCF
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

//
// The startup function first tries to detect whether deskHPSDR is running
// in its own directory (as usual for compiled-from-the-source installations),
// or whether it is started from a desktop icon and resides in /usr/bin or
// /usr/local/bin.
//
// Only in the latter case, the following steps are taken (this eliminates the
// need for a wrapper startup script):
//
// - create a working directory, if it not yet exists
// - make this the current working directory
//
// The working directory on Linux is $HOME/.config/deskhpsdr, on MacOS it is
// "$HOME/Library/Application Support/deskHPSDR".
//
// If something goes wrong (e.g. the $HOME environment variable does not exist,
// the working directory exists but is not a directory, or cannot be created)
// then $HOME is used as the working dir.
//
// Note no output (via t_print) should be made until either stdout is "reconnected"
// or we know that we won't reconnect it.
//
// This routine is also the right place to set priorities, etc., if the operating
// system allows. For MacOS, we set the "Keep awake" flag.
//

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pwd.h>

#ifdef __APPLE__
  #include <IOKit/IOKitLib.h>
  #include <IOKit/pwr_mgt/IOPMLib.h>
#endif

#include "message.h"

char workdir[PATH_MAX];

//
// Number of previous runs kept beside the current one, as deskhpsdr.log.1 and
// so on. Raise it if you restart several times before noticing a problem,
// lower it if the logs get big, which they do with the debug options on.
//
#define LOG_GENERATIONS 5

//
// Rotate a log instead of truncating it.
//
// stdout and stderr used to be reopened with "w", so every start erased the
// log of the run before it. That is precisely the run worth reading: a fault
// is noticed after the restart that destroys the evidence of it, and the
// comment below still calls these files post-mortem debugging.
//
// The oldest generation is dropped, the others shift up by one, and the run
// that just ended becomes .1. Nothing here can fail in a way worth reporting,
// and there is no way to report it anyway: stdout is not connected yet.
//
static void rotate_logfile(const char *name) {
  char from[PATH_MAX];
  char to[PATH_MAX];
  struct stat statbuf;

  if (stat(name, &statbuf) < 0) {
    return;                          // no log from a previous run
  }

  snprintf(to, sizeof(to), "%s.%d", name, LOG_GENERATIONS);
  (void) remove(to);

  for (int i = LOG_GENERATIONS - 1; i >= 1; i--) {
    snprintf(from, sizeof(from), "%s.%d", name, i);
    snprintf(to, sizeof(to), "%s.%d", name, i + 1);
    (void) rename(from, to);         // fails harmlessly when .i does not exist
  }

  snprintf(to, sizeof(to), "%s.1", name);
  (void) rename(name, to);
}

static void reopen_logfile(const char *name, FILE *stream) {
  rotate_logfile(name);

  //
  // Nothing to do if this fails, and no way to say so: the stream is gone
  // either way and there is no output channel left to complain through.
  // Assigning the result is what keeps freopen's warn_unused_result quiet,
  // a cast to void does not.
  //
  if (freopen(name, "w", stream) == NULL) {
    return;
  }
}

void startup(const char *path) {
  struct stat statbuf;
  int rc;
  const char *homedir;
  const struct passwd *pwd;
#ifdef __APPLE__
  static IOPMAssertionID keep_awake = 0;
  //
  //  This is to prevent "going to sleep" or activating the screen saver
  //  while deskHPSDR is running
  //
  //  works from macOS 10.6 so no check on availability needed.
  //  no return check is needed: if it fails, it fails.
  //
  IOPMAssertionCreateWithName(kIOPMAssertionTypeNoDisplaySleep, kIOPMAssertionLevelOn,
                              CFSTR("deskHPSDR"), &keep_awake);
#endif
  //
  // Get home dir
  //
  homedir = getenv("HOME");
  if (homedir == NULL) {
    pwd = getpwuid(getuid());
    if (pwd != NULL) {
      homedir = pwd->pw_dir;
    }
  }
  if (homedir == NULL) {
    // If $HOME cannot be determined: stay in CWD if writable, otherwise fall back to /tmp/deskhpsdr.
    if (access(".", W_OK) == 0) {
      return;
    }
    g_strlcpy(workdir, "/tmp/deskhpsdr", PATH_MAX);
    if (stat(workdir, &statbuf) < 0) {
      if (mkdir(workdir, 0755) != 0) {
        t_print("%s: Could not create %s\n", __func__, workdir);
        return;
      }
    } else if (!S_ISDIR(statbuf.st_mode)) {
      t_print("%s: %s exists but is not a directory\n", __func__, workdir);
      return;
    } else {
      if (chmod(workdir, 0755) != 0) {
        t_print("%s: Could not chmod %s to 0755\n", __func__, workdir);
        return;
      }
    }
    if (chdir(workdir) != 0) {
      t_print("%s: Could not chdir to working dir %s\n", __func__, workdir);
    } else {
      reopen_logfile("deskhpsdr.log", stdout);
      reopen_logfile("deskhpsdr.err", stderr);
      t_print("%s: working dir changed to %s\n", __func__, workdir);
      t_print("%s: previous log kept as deskhpsdr.log.1 (up to %d generations)\n",
              __func__, LOG_GENERATIONS);
    }
    return;
  }
#ifdef __APPLE__
  snprintf(workdir, PATH_MAX, "%s/Library/Application Support/deskHPSDR", homedir);
  if (stat(workdir, &statbuf) < 0) {
    mkdir(workdir, 0700);
  }
  rc = stat(workdir, &statbuf);
  if (rc < 0 || !S_ISDIR(statbuf.st_mode)) {
    g_strlcpy(workdir, homedir, PATH_MAX);
  }
#else
  snprintf(workdir, PATH_MAX, "%s/.config", homedir);
  if (stat(workdir, &statbuf) < 0) {
    mkdir(workdir, 0700);
  }
  snprintf(workdir, PATH_MAX, "%s/.config/deskhpsdr", homedir);
  if (stat(workdir, &statbuf) < 0) {
    mkdir(workdir, 0700);
  }
#endif
  //
  // Check if workdir exists and is a directory, if not, take home dir
  //
  rc = stat(workdir, &statbuf);
  if (rc < 0 || !S_ISDIR(statbuf.st_mode)) {
    g_strlcpy(workdir, homedir, PATH_MAX);
  }
  //
  // At this point, the new working directory exists and the name
  // is in filename.
  //
  if (chdir(workdir) != 0) {
    // unrecoverable error, could not chdir to target
    t_print("%s: Could not chdir to working dir %s\n", __func__, workdir);
    return;
  }
  //
  //  Make two local files for stdout and stderr, to allow
  //  post-mortem debugging
  //
  reopen_logfile("deskhpsdr.log", stdout);
  reopen_logfile("deskhpsdr.err", stderr);
  t_print("%s: working dir changed to %s\n", __func__, workdir);
  t_print("%s: previous log kept as deskhpsdr.log.1 (up to %d generations)\n",
          __func__, LOG_GENERATIONS);
}

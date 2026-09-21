---
artifact_contract: "ce-handoff/v1"
created_at: "2026-09-21T18:31:47Z"
title: "glib 2.90.0 is the CW keying fault, and the plan to bundle GTK"
summary: "The unusable CW keying was traced to Homebrew glib 2.90.0, not to the branch; proved by running the same commit against the official bundle's glib 2.88.0, and the chosen fix is to make deskHPSDR carry its own GTK stack."
keywords: ["deskhpsdr", "glib", "homebrew", "gtk", "app-bundle", "cw", "keyer", "macos", "dylib"]
cwd: "/Users/sf/Developer/deskhpsdr/.claude/worktrees/confident-hopper-0653a7"
resume_focus: "Implement solution A: populate Contents/Frameworks in install-Darwin so deskHPSDR ships its own GTK stack, after deciding where a working glib comes from"
repository: "iu3qez/deskhpsdr"
repo_root_sha: "5ecf980078506aa9eb523b8b8479627a0843088a"
branch: "claude/midi-paddle-lag-3acf29"
head: "fcb108eb5f051c38b685a337365c5bfb413131d9"
worktree_path: "/Users/sf/Developer/deskhpsdr/.claude/worktrees/confident-hopper-0653a7"
---

# Handoff - glib 2.90.0 is the CW keying fault, and the plan to bundle GTK

Date: 2026-09-21. This session started as a resume of
`docs/handoffs/2026-09-20-cw-keying-off-main-loop-handoff.md` to continue the CW
keying work at U3. It did not get there. Read that handoff for the CW plan
itself; this one records why the plan is now on hold and what replaced it.

## What this session established

The unusable CW keying is **Homebrew glib 2.90.0**, not the branch.

The proof is a controlled experiment, not an inference. The same commit
`fcb108eb`, same binary, linked against the official release bundle's GTK stack
instead of Homebrew's:

| | Homebrew stack | Official bundle stack |
|---|---|---|
| glib | 2.90.0 (dylib current `9001.0.0`) | 2.88.0 (dylib current `8801.3.0`) |
| GTK | 3.24.52 | 3.24.32 |
| pango | 1.58.2 | 1.54.0 |
| Keying | unusable | good, per the user at the radio |
| `DIAG mox wait` | 8 of ~75 events over 200 ms, worst 1.9 s | 54 ms and 60 ms, no `TIMEOUT` |

The dylib version field encodes `100*minor + micro + 1`. Calibrated on the known
value: Homebrew glib is 2.90.0 and records `9001`, so `8801` is 2.88.0.

glib 2.90.0 was installed **2026-09-18 19:48**, the same minute as
at-spi2-core 2.62.0.1. That timestamp matches the GUI stalls the user had
already recorded in memory as starting that evening.

## The second result, which changes what the CW plan is worth

From the run against glib 2.88.0, in
`~/Library/Application Support/deskHPSDR/deskhpsdr.log` (machine-local, rotates):

```
rxtx_rf:      DIAG rx flush 47 ms, rxtx_rf(1) total 47 ms, receivers=1
keyer_thread: DIAG mox wait 60 ms  moxbefore=0 mox=1 cw_not_ready=0
```

Decompose the 60 ms: `rxtx_rf` is 47 ms of it, the `cw_not_ready` handshake needs
about 1.3 ms for one P2 mic sample, leaving roughly 12 ms of GTK main-loop queue
time. The 47 ms is the WDSP receiver flush, `rx_begin_off` and `rx_wait_off`,
which runs inside `rxtx_rf()` whichever thread calls it.

So on a healthy glib the first-dit delay is dominated by the receiver flush, not
by the main loop. U3 and U4 of the CW plan remove the ~12 ms and make keying
immune to stalls, which is still worth having, but they will not remove the
~50 ms. That is why the small first-dit glitch survives in the official binary
too. **The plan's Success Criteria were calibrated on glib 2.90.0 and assumed the
main loop was the dominant term. It is not.** This is my reading of the
measurement, not something the user stated.

## The chosen fix: solution A, bundle GTK with the app

The user chose this over downgrading Homebrew and over Nix. Rejected alternatives
and why are in the section below.

**The repository does not do this today.** `install-Darwin` at
`Makefile:1196-1237` creates `deskHPSDR.app/Contents/Frameworks` at
`Makefile:1208` and then never copies a dylib into it. There is no
`install_name_tool` step and no loaders cache step. The resulting app still binds
Homebrew's GTK at absolute paths. dl1bz builds his release bundles by some
process that is not in this repository.

What a working bundle needs, each step verified against the official bundle
tonight:

1. Copy the GTK stack dylibs into `Contents/Frameworks`. The binary needs 12
   directly; the official bundle ships 35 including transitive dependencies and
   the `libpixbufloader-*.so` modules.
2. Rewrite load commands to `@executable_path/../Frameworks/...`. Some upstream
   dylibs carry `@loader_path` install names, which resolve to the executable's
   own directory and are wrong for a main executable. `dylibbundler` automates
   steps 1 and 2.
3. Generate `Contents/Resources/gdk-pixbuf-loaders.cache` with
   `gdk-pixbuf-query-loaders`, and copy the glib schemas to
   `Contents/Resources/share/glib-2.0/schemas`.
4. Compile with `-DBUNDLED_APP`. `src/main.c:995-1016` then sets
   `GSETTINGS_SCHEMA_DIR`, `GDK_PIXBUF_MODULEDIR` and `GDK_PIXBUF_MODULE_FILE`
   from the bundle. The flag appears nowhere in the Makefile, so only dl1bz's
   release builds define it. Its only other effect is adding the string
   `APP-BUNDLE` to a version banner at `src/version.c:50`.
5. `codesign --force --deep --sign -`, which `install-Darwin` already does at
   `Makefile:1229`. Note that `install_name_tool` invalidates an existing ad-hoc
   signature, so signing must come after the rewrite.

**Open question that blocks A, and the next agent must not skip it.** Bundling
makes the app immune to `brew upgrade`; it does not by itself supply a *working*
glib. Bundling the current Homebrew glib 2.90.0 would bundle the fault. A needs a
source for glib 2.88.x: build it from source into a private prefix, or copy the
dylibs out of dl1bz's release bundle, or wait for the regression to be fixed
upstream. Tonight's working instance took the second route, which is fine for a
test but depends on the official app being present on the machine.

## A working reference implementation exists

`oldglib-test/` in this worktree (machine-local, untracked, disposable) is the
branch running on glib 2.88.0. It is the runtime half of solution A, already
proven:

- `oldglib-test/Contents/Frameworks` and `.../Resources` are **symlinks** into
  `/Users/sf/Downloads/deskHPSDR.app`. The official app is not modified.
- `oldglib-test/Contents/MacOS/deskhpsdr` is our build, its load commands
  rewritten and the binary re-signed ad-hoc.
- `oldglib-test/run.sh` sets the three environment variables that
  `-DBUNDLED_APP` would set, and is the model for step 4.

It was built by overriding one Makefile variable, no source or Makefile edit:

```
make -j8 GTK_LIBS="<full paths to the bundle dylibs>" deskhpsdr
```

`GTK_LIBS` is a plain assignment at `Makefile:567`, so a command-line override
replaces it. Headers stayed Homebrew's, only the link target changed, and it
linked with no missing symbols. The version banner still prints "GTK+ version
3.24.52" because that is the compile-time macro, not what is loaded; the load
commands are the authority.

## The recurrence vector, worth fixing regardless of A

`update_libs.sh:69` runs `brew update` and `brew upgrade` unconditionally, with
no version constraint on anything. That is almost certainly what installed glib
2.90.0 on 2026-09-18, and it pulled harfbuzz to 14.5.0 and librsvg to 2.63.2 on
2026-09-21 at 18:18. The script only needs its own `brew install` lines. Removing
the `brew upgrade` is a one-line change in the fork and a clean small pull
request for dl1bz, since it is a footgun for every macOS user.

The script is reached through `all: prepare $(PROGRAM)` at `Makefile:1199` with
`.DEFAULT_GOAL := all`, guarded by the `.WDSP_libs_updated_V3` checkfile. The
`deskhpsdr`, `test` and `cppcheck` targets do not depend on `prepare`.

## Alternatives considered and rejected

- **Docker.** Rejected on three hard blockers, not preference. The paddle arrives
  over CoreMIDI and Docker Desktop on macOS has no USB or CoreMIDI passthrough.
  The build links CoreAudio, AudioToolbox and AudioUnit and is compiled with
  `-DCOREAUDIO`. GTK in a container needs X11 to XQuartz or VNC. All three add
  latency to a problem measured in tens of milliseconds.
- **Downgrade glib in Homebrew and pin.** A glib-only downgrade will not boot.
  `libgtk-3.0.dylib` and `libpango-1.0.0.dylib` require glib compat `8801`, but
  `libatk-1.0.0.dylib` (at-spi2-core), `libharfbuzz.0.dylib` and
  `librsvg-2.2.dylib` require `9001`, and GTK 3 hard-links ATK. The minimum set
  is glib plus those three, none of which have an older copy in the Cellar or the
  bottle cache, so each means `brew extract` plus a source build. Homebrew also
  resolves dependencies by formula name, so a tapped `glib@2.88.0` is awkward to
  make satisfy everyone else's plain `glib`.
- **Nix with a pinned nixpkgs.** Not rejected on merit; the user chose A. It
  remains the only option that is reproducible by construction.

## Current state of the code

- **HEAD is `fcb108eb`**, branch `claude/midi-paddle-lag-3acf29`, 5 commits ahead
  of `master`, nothing pushed, no PR. U1 and U2 are commits `db22c382` and
  `57c45dc7`.
- **One uncommitted change**, `src/radio.c`, 14 insertions and 1 deletion. It
  queues the GUI converger with `g_idle_add_full(G_PRIORITY_LOW, ...)` instead of
  plain `g_idle_add`. Build, `make cppcheck` and `make test` were clean on it
  (3544 + 13 checks, 0 failed). It is **not** the fix for the glib fault; it
  fixes a real ordering regression U2 introduced, described below.
- **U3 to U6: not started.** No mutex, no real-time entry point. The bounded
  `tx_off` wait the user chose for U3 was never written.

### The U2 ordering regression, fixed but uncommitted

Worth keeping even though glib turned out to dominate. The keyer posts
`ext_mox_update` with plain `g_idle_add` at `src/iambic.c:365`, which is
`G_PRIORITY_DEFAULT_IDLE`. U2 queued the converger at the same priority, so a
re-key request landed behind a pending panel restore. Before U2 the panel work ran
inline inside `rxtx()` and was complete before `mox` was assigned, so the keyer
could not observe the transition until the interface had settled. The fix lowers
the converger to `G_PRIORITY_LOW`, which is safe because the converger takes no
argument and converges to current state (KTD2 in the plan).

Unrelated and pre-existing: `rx_set_displaying()` at `src/receiver.c:742` and
`tx_set_displaying()` at `src/transmitter.c:2136` arm their display timers at
`G_PRIORITY_HIGH_IDLE`, which outranks the keyer's request. Not touched.

## Fragile local state, all machine-local

- `/Users/sf/Developer/deskhpsdr/.claude/worktrees/confident-hopper-0653a7` -
  this worktree. Its `wdsp-libs` symlink was **destroyed** today by a bare `make`
  and has been restored. `.WDSP_libs_updated_V3` was created here to stop it
  happening again. Other worktrees still lack that checkfile.
- `wdsp-libs.broken-20260921/` - 202 MB, the half-built rnnoise clone that
  replaced the symlink. Moved aside rather than deleted, safe to remove.
- `deskhpsdr.homebrew` - the previous Homebrew-linked binary, kept for A/B.
- `oldglib-test/` - described above. Depends on
  `/Users/sf/Downloads/deskHPSDR.app` existing.
- **Never run a bare `make`**, and never build in a terminal where ESP-IDF's
  `export.sh` has been sourced. That puts
  `/Users/sf/.espressif/tools/esp32ulp-elf/2.38_20240113/esp32ulp-elf/esp32ulp-elf/bin/ar`
  ahead of `/usr/bin/ar`. It is GNU binutils 2.38 and writes GNU-format archives
  whose long-name member is called `//`, which Apple's `ld` cannot read. That is
  the whole `librnnoise.a` link failure seen today, and it has nothing to do with
  deskHPSDR.

## Verification performed

Build, `make cppcheck` and `make test` clean on the uncommitted `src/radio.c`
change. The hybrid bundle was verified statically: all 16 GTK dylibs resolve
under `Contents/MacOS`, no `@loader_path` entries remain, no Homebrew GTK
reference remains, and `codesign -v` passes. Runtime verification is the user's
own keying at the radio plus the log, which showed no `TIMEOUT` and no GTK or
GLib warning.

Not verified: any part of solution A beyond the runtime behaviour proven by
`oldglib-test/`. Nothing in the Makefile has been changed.

## Decisions and their owners

The user decided:

- Restore a working library stack before continuing the CW work.
- Solution A, bundle GTK with the app, over the Homebrew downgrade and over Nix.
- Earlier in the session, for U3: keep `tx_off_cancel_target()` on the real-time
  path and make its wait bounded, extending U3 to `src/tx_off.c` and
  `src/tx_off.h`. That decision stands but no code was written.

My own calls, which the next agent may revisit:

- Lowering the converger to `G_PRIORITY_LOW` rather than reverting U2.
- Moving the broken `wdsp-libs` directory aside instead of deleting it.
- Building the hybrid against the official bundle's dylibs, with Homebrew
  headers, rather than building a private GTK prefix. It was the cheapest way to
  isolate the variable and it linked without missing symbols, but compiling
  against 2.90 headers while linking 2.88 dylibs is not a configuration to ship.

## Where this can go next

One sequential path:

1. Decide where a working glib 2.88.x comes from. This blocks everything else in
   A. The three candidates are in the solution A section.
2. Implement the bundling steps in `install-Darwin`, using `oldglib-test/` as the
   reference for the runtime half and `dylibbundler` for the mechanical half.
3. Remove `brew upgrade` from `update_libs.sh` and consider the upstream pull
   request.
4. Only then return to the CW plan at U3, with the Success Criteria re-derived
   against a healthy main loop. `docs/plans/2026-09-20-2006-fix-cw-keying-off-main-loop-plan.md`
   is still the reviewed plan; its Implementation Constraints table is unaffected
   by any of today's findings.

Separately and not blocking: the glib 2.90.0 main-loop regression itself is worth
a minimal reproducer and an upstream report. A small GLib program that arms a
`g_timeout_add` and records actual dispatch times would be enough. This was
offered during the session and the user did not take it up.

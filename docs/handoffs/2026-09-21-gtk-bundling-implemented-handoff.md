---
artifact_contract: "ce-handoff/v1"
created_at: "2026-09-21T20:01:40Z"
title: "GTK is now bundled in the .app, and the brew upgrade footgun is gone"
summary: "Solution A is implemented and verified: GTK_BUNDLE makes deskHPSDR link and ship its own glib 2.88 stack, update_libs.sh no longer upgrades every Homebrew formula, and the CW plan is unblocked at U3 with its success criteria still needing re-derivation."
keywords: ["deskhpsdr", "gtk", "glib", "app-bundle", "dylib", "install_name_tool", "homebrew", "cw", "keyer", "macos", "makefile"]
cwd: "/Users/sf/Developer/deskhpsdr/.claude/worktrees/confident-hopper-0653a7"
resume_focus: "Return to the CW plan at U3, re-deriving its Success Criteria against a healthy glib where the ~47 ms WDSP receiver flush, not the GTK main loop, dominates the first-dit delay"
repository: "iu3qez/deskhpsdr"
repo_root_sha: "5ecf980078506aa9eb523b8b8479627a0843088a"
branch: "claude/midi-paddle-lag-3acf29"
head: "bdf2e7a98debb5e2aaac1b1d3a6bc7d2a490150c"
worktree_path: "/Users/sf/Developer/deskhpsdr/.claude/worktrees/confident-hopper-0653a7"
---

# Handoff - GTK is bundled, the brew upgrade is gone

Date: 2026-09-21, later the same day as
`docs/handoffs/2026-09-21-glib-290-and-gtk-bundling-handoff.md`. That handoff
diagnosed the fault and chose solution A. This one records that A is built,
verified and committed, and what it changed about the plan.

Read the earlier handoff for the diagnosis and the A/B measurement. Nothing in
it is contradicted here; two of its five implementation steps turned out to be
unnecessary, which is recorded below.

## What this session produced

Two commits, both on `claude/midi-paddle-lag-3acf29`, neither pushed:

| Commit | Subject |
|---|---|
| `3e0e4cea` | `build: let the macOS app carry its own GTK stack` |
| `bdf2e7a9` | `build: drop the unconditional brew upgrade from update_libs.sh` |

`3e0e4cea` touches `Makefile` and `.gitignore` only. No C source was changed in
this session.

### How the bundling works

`GTK_BUNDLE` is one opt-in Make variable, empty by default. The whole feature is
in `Makefile`:

- **Definition block**, `Makefile:579-641`. Sets `GTK_LIBS` to the 16 dylibs
  deskhpsdr links directly, and `BUNDLE_DEFINES` to `-DBUNDLED_APP` plus the two
  `GLIB_VERSION_*` macros. Read the comment there first: it records why the
  dylibs must be linked rather than relinked.
- **`GTK_LIBS_NATIVE`**, `Makefile:575`. A `:=` snapshot of the pkg-config value
  taken before `GTK_BUNDLE` can replace it.
- **`install-Darwin`**, guard at `Makefile:1335-1348`, bundling at
  `Makefile:1371-1390`. The guard runs before the recipe's first `rm -rf`. The
  bundling copies `Frameworks/` and `Resources/`, rewrites the executable's
  `@loader_path` load commands, then fails the build if any reference outside
  the bundle survives.
- **`gtk-bundle-seed`**, `Makefile:1299`. Copies a stack out of an existing
  `.app`. Reads the source only.
- **`gtk-bundle-check`**, `Makefile:1272`. Compiles every source with the
  glib pin and `-Wdeprecated-declarations`, keeps only the pin's own message.

With `GTK_BUNDLE` empty, `make -n deskhpsdr` carries none of these flags. That
was checked, not assumed.

The runtime half needed no new code: `setup_macos_bundle_environment()` at
`src/main.c:982-1018` already sets `GSETTINGS_SCHEMA_DIR`,
`GDK_PIXBUF_MODULEDIR` and `GDK_PIXBUF_MODULE_FILE` under `-DBUNDLED_APP`, and
`src/version.c:51` adds `APP-BUNDLE` to the banner. Only the Makefile had to
define the macro.

### Three findings that shortened the plan

The earlier handoff listed five bundling steps. Two are not needed, and one
constraint removes a tempting shortcut.

1. **`dylibbundler` is not needed.** The dylibs in dl1bz's bundle reference each
   other as `@loader_path/X`, and `@loader_path` for a dylib living in
   `Contents/Frameworks` already resolves to `Contents/Frameworks`. Only the
   executable, in `Contents/MacOS`, resolves them wrongly. So step 1 is `cp -R`
   and step 2 touches the executable alone. The tool was never installed.
2. **`gdk-pixbuf-query-loaders` and `glib-compile-schemas` are not needed.**
   `Contents/Resources/gdk-pixbuf-loaders.cache` in the official bundle already
   contains `@executable_path/../Frameworks/libpixbufloader-*.so`, and
   `share/glib-2.0/schemas/gschemas.compiled` ships with it. Both are copies.
3. **Linking against Homebrew and rewriting paths afterwards does not work.**
   dyld refuses a dylib whose compatibility version is lower than the load
   command records. A Homebrew build records `9001.0.0` for libglib where the
   2.88 dylib provides `8801.0.0`, so the rewritten binary would fail at launch.
   The link target itself has to change. This is why `GTK_BUNDLE` affects the
   build and not only `install-Darwin`, and it is the thing a next agent is most
   likely to try to simplify away.

## Decisions and their owners

The user decided, in this session:

- The dylibs are **vendored into the worktree**, not read from
  `/Users/sf/Downloads/deskHPSDR.app` at build time, and not committed to the
  fork. Directory `macos-gtk-2.88/`, 29 MB, ignored through the
  `macos-gtk-*/` rule added to `.gitignore`.
- The glib API level is **pinned to 2.88**, as a warning rather than an error.
- `install-Darwin` was run for real, to `~/Desktop`, rather than to a staging
  directory.
- Step 3 of the earlier handoff, removing `brew upgrade`, was done now.

My own calls, which the next agent may revisit:

- **The pin lives in `gtk-bundle-check`, not in the build.** `CFLAGS` carries
  `-Wno-deprecated-declarations`, which is exactly the warning glib's version
  macros raise, so the pin was inert as first written. Arming it in the build
  would add 29 pre-existing GTK 3 deprecation warnings to every compile and bury
  the one signal. The separate target arms it and filters for
  `warning:.*Not available before`. The user asked for a warning, not an error,
  so the target is not wired into `all` or `test`.
- **`GTK_LIBS_NATIVE` for `property-test`**, `Makefile:1145`. That binary runs
  from the source directory and cannot resolve
  `@executable_path/../Frameworks`. Without the split, `make test` breaks
  whenever `GTK_BUNDLE` is set.
- **`brew update` was kept**, only `brew upgrade` removed. The `brew install`
  lines below it need current formulae.
- **The 16-dylib list is explicit** rather than derived from pkg-config, because
  the pkg-config `-L`/`-l` form would resolve back to Homebrew.

## Current state

**Complete and verified.** The bundling, the seed and check targets, the
`update_libs.sh` change.

**Not started.** U3 to U6 of
`docs/plans/2026-09-20-2006-fix-cw-keying-off-main-loop-plan.md`. No mutex, no
real-time entry point, and the bounded `tx_off` wait the user chose for U3,
extending to `src/tx_off.c` and `src/tx_off.h`, was never written. That decision
still stands from the previous session.

**Unchanged and still true.** The plan's Implementation Constraints table is
unaffected. Its Success Criteria are not: they were calibrated on glib 2.90.0
and assume the GTK main loop dominates the first-dit delay. On a healthy glib
roughly 47 ms of the measured 60 ms is the WDSP receiver flush inside
`rxtx_rf()`, about 1.3 ms is the `cw_not_ready` handshake, and only about 12 ms
is main-loop queue time. U3 and U4 buy back that 12 ms and immunity to stalls,
not 50 ms. This decomposition is the previous session's reading of one log
sample, not a figure the user stated, and it has not been re-measured since.

## Verification performed

On the bundled build:

- Clean rebuild: links glib 2.88, only two pre-existing `src/rx_menu.c`
  unused-variable warnings.
- `make gtk-bundle-check GTK_BUNDLE=macos-gtk-2.88`: no glib API newer than
  2.88. Proven to work in the failing direction too, by pinning to
  `GLIB_VERSION_2_26`, which exits 2 and reports real hits such as `g_thread_new`
  at `src/cw_engine.c:515`.
- `make test GTK_BUNDLE=macos-gtk-2.88`: 3544 + 13 checks, 0 failed, with
  `property_test` correctly linked against pkg-config.
- `make cppcheck GTK_BUNDLE=macos-gtk-2.88`: exit 0. Output is byte-identical to
  the run without the bundle defines, 4 errors and 43 warnings, all pre-existing.
  Nothing is reported inside the newly linted
  `setup_macos_bundle_environment()`.
- `otool -L` on the installed executable: no reference outside
  `@executable_path/../Frameworks`, `/System` and `/usr/lib`.
- `codesign --verify --deep --strict`: passes after `install_name_tool` and
  after the move to `~/Desktop`.
- Runtime: the app launched, built its GUI, ran discovery and showed the device
  dialog with no GTK, GLib, pixbuf or schema warnings in
  `~/Library/Application Support/deskHPSDR/deskhpsdr.log` (machine-local,
  rotates). The user confirmed the app is good.

**Not verified, and this is the gap that matters.** Keying was not re-measured
at the radio against the bundled build. The radio was unreachable during the
session, `No route to host`, discovery found 0 devices, so no new
`DIAG mox wait` samples exist. The evidence that the bundled stack fixes keying
is still the previous session's A/B against the same dylibs, not a measurement
of this binary.

## Failed approaches, so they are not retried

- **Counting deprecation warnings with `cc -fsyntax-only` and a hand-assembled
  flag list.** I did this to size the cost of arming the pin and got zero
  warnings tree-wide. The compiles were failing on `src/radio.c:42`,
  `'wdsp.h' file not found`, and I had not checked the exit status. The real
  figure is 29. Use `$(COMPILE)`, which is what `gtk-bundle-check` does, or read
  a real build.
- **Relinking a Homebrew-built binary onto the 2.88 dylibs.** Blocked by the
  compatibility version, described above.
- **`make clean` before rebuilding.** It recurses into `wdsp-2.10`, `miniaudio`,
  `rtmidi`, `libsolar` and `libtelnet`. Removing `src/*.o src/*.d deskhpsdr` is
  enough after a flag change and does not touch the fragile `wdsp-libs` symlink.

## Fragile local state, all machine-local

- `/Users/sf/Developer/deskhpsdr/.claude/worktrees/confident-hopper-0653a7` -
  this worktree. Its `wdsp-libs` symlink and `.WDSP_libs_updated_V3` sentinel are
  both present, so `prepare` is a no-op here and `install-Darwin` does not
  re-run `update_libs.sh`.
- `macos-gtk-2.88/` - the vendored stack, 29 MB, gitignored, seeded from
  `/Users/sf/Downloads/deskHPSDR.app`. **Required for any `GTK_BUNDLE` build.**
  Re-seed with
  `make gtk-bundle-seed GTK_BUNDLE=macos-gtk-2.88 GTK_BUNDLE_FROM=/path/to/deskHPSDR.app`.
- `/Users/sf/Downloads/deskHPSDR.app` - dl1bz's release, the only source of 2.88
  dylibs on this machine. Still needed to re-seed, no longer needed to build.
- `~/Desktop/deskHPSDR.app` - the bundled build, installed and launched.
- `oldglib-test/`, `deskhpsdr.homebrew`, `wdsp-libs.broken-20260921/` (202 MB) -
  all superseded by `3e0e4cea`, left in place, safe to remove.
- Homebrew glib is still 2.90.0. That no longer reaches the app, but it still
  affects anything else linking Homebrew GTK, including `property_test`.
- `cppcheck` was missing at the start of this session and was installed during
  it, version 2.22.0. `make cppcheck` is slow here because `--check-level=exhaustive`
  is set for Darwin at `Makefile:1112`, `--enable=all` is on, and no `-j` is
  passed.
- Never build in a shell where ESP-IDF's `export.sh` has been sourced; it puts a
  GNU `ar` ahead of `/usr/bin/ar` and breaks the `librnnoise.a` link.

## Where this can go next

One sequential path, which is the previous handoff's step 4:

1. Re-derive the Success Criteria in
   `docs/plans/2026-09-20-2006-fix-cw-keying-off-main-loop-plan.md` against a
   healthy main loop, ideally after collecting fresh `DIAG mox wait` samples
   from the bundled build with the radio reachable.
2. Implement U3, the bounded `tx_off` wait, in `src/tx_off.c` and
   `src/tx_off.h`, then U4.

Two independent items, neither blocking:

- The `update_libs.sh` change in `bdf2e7a9` is a clean single-commit pull
  request for dl1bz at `upstream`, `https://github.com/dl1bz/deskhpsdr.git`. It
  is a footgun for every macOS user. It would need its own branch cut from
  upstream master, since this branch also carries unfinished CW work. Note the
  separate standing decision not to propose the CW-on-serial work upstream until
  the Linux `TIOCMIWAIT` path exists.
- The glib 2.90.0 main-loop regression still has no minimal reproducer and no
  upstream report. A small GLib program arming a `g_timeout_add` and recording
  actual dispatch times would be enough. Offered twice now, not taken up.

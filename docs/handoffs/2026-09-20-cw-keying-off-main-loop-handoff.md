---
artifact_contract: "ce-handoff/v1"
created_at: "2026-09-20T19:40:00Z"
title: "CW keying off the GTK main loop"
summary: "Lost first CW dit diagnosed as GTK main-loop stalls; plan written and reviewed, units U1 and U2 of six implemented and committed, nothing verified on the air yet."
keywords: ["deskhpsdr", "cw", "iambic", "keyer", "rxtx", "gtk-main-loop", "orion2", "protocol-2"]
cwd: "/Users/sf/Developer/deskhpsdr/.claude/worktrees/confident-hopper-0653a7"
resume_focus: "Verify U1 and U2 on the air, then implement U3 to U6 of the plan"
repository: "iu3qez/deskhpsdr"
repo_root_sha: "5ecf980078506aa9eb523b8b8479627a0843088a"
branch: "claude/midi-paddle-lag-3acf29"
head: "57c45dc7e8eb85e30e72604755aef188a596f10f"
worktree_path: "/Users/sf/Developer/deskhpsdr/.claude/worktrees/confident-hopper-0653a7"
---

# Handoff - CW keying off the GTK main loop

Date: 2026-09-20. Written in English per the user's global instruction, unlike the two earlier handoffs in this directory.

## Objective

The first CW element of a transmission is sometimes sent with no RF: deskHPSDR switches to TX on screen, the operator hears no dit. The work is to make CW break-in start and stop transmit without waiting for the GTK main loop. Radio is an ANAN Orion2 on Protocol 2, paddle on a MIDI adapter.

## What the diagnosis established

Measured, not inferred. Temporary `DIAG` lines in `src/iambic.c` and `src/radio.c` over about 75 keying events across two builds:

- The keyer posts `ext_mox_update` with `g_idle_add` (`src/iambic.c:365`) and waits at most 200 ms for `mox && !cw_not_ready` (`src/iambic.c:374`). On timeout it keys anyway, and `tx_add_mic_sample()` counts `cw_key_down` down while still receiving (`src/transmitter.c:2073-2075`), so the element is consumed without RF.
- The transmit switch itself costs 21-53 ms, of which 21-46 ms is the WDSP receiver flush. The rest of the keyer's wait is main-loop queue time.
- Eight of about 75 events exceeded the 200 ms budget. Worst case: the main loop ran nothing for about 1.9 s, then three queued transmit requests and their releases executed back to back.
- A build from before the 2026-09-18 upstream merge behaves identically, so RtMidi, the miniaudio backend and the WDSP flush change are all ruled out as the cause.

Two things were ruled out along the way and should not be re-investigated: the RtMidi backend replacement (both backends deliver the paddle synchronously on the CoreMIDI callback thread), and the audio output device (the user tested, symptom unchanged).

The main-loop stalls themselves correlate with the Homebrew **glib 2.90.0** upgrade of 2026-09-18 19:48. **The user decided the stall is out of scope** - keying must not depend on the interface thread whatever the cause. There is a memory note with the details.

## Authoritative references

- `docs/plans/2026-09-20-2006-fix-cw-keying-off-main-loop-plan.md` - the plan. Read the Goal Capsule, then the Implementation Constraints table in the Planning Contract: it classifies every statement of the old `rxtx()` as RF-half or GUI-half work, and it is what U3 to U6 depend on. Its line numbers are against the tree **with** the `DIAG` lines present; removing them shifts `src/radio.c` up by about nine.
- `src/radio.c`, the region from `rxtx_rf()` to `rxtx()` - the split as it now exists. `radio_tx_gui_sync()` is the argument-less converger; `radio_tx_notify()` is the seam U3 will queue from the keyer thread.
- `src/iambic.c:325-408` - the keyer thread, its 200 ms and 250 ms waits, and the two `g_idle_add` calls U4 replaces.
- `src/cw_engine.c:412, 444, 466, 488, 506, 536` - the one request and five release sites U5 must convert together.
- `docs/handoffs/2026-09-10-tci-remoting-and-totw-handoff.md` - unrelated work, but it is the house style for these documents.

## Work completed

Four commits on `claude/midi-paddle-lag-3acf29`, none pushed, no PR:

| Commit | Contents |
|---|---|
| `2c186a46` | the plan document |
| `f5a0b859` | the temporary `DIAG` instrumentation, committed separately so unit commits stay clean |
| `db22c382` | U1: `tx_on()`/`tx_off()` set the WDSP channel state only; the level window moved to the callers, and `tx_levels_show()`/`tx_levels_hide()` lost their `static inline` and are declared in `src/transmitter.h` |
| `57c45dc7` | U2: `rxtx()` split into `rxtx_rf()` and `radio_tx_gui_sync()`, all five call sites routed through both halves, the MOX notifications moved into `radio_tx_notify()` |

The plan itself went through `ce-plan` and then `ce-doc-review` with five reviewer lenses. Seventeen findings were applied to it before implementation started, so the plan in the tree is the reviewed version, not the first draft. The heaviest ones: the transmit switch has five call sites rather than two; the original latency criterion was unreachable; and the acceptance check depended on a log marker the cleanup step deletes.

## Current state

- **U1 and U2: complete, not verified at runtime.** `make -j8 deskhpsdr` clean, `make cppcheck` reports nothing in the changed files, `make test` green (3544 + 13 checks). Checked by hand that no GTK call remains in the RF half and no WDSP or protocol call entered the converger.
- **U3 to U6: not started.** The mutex and real-time entry point, the paddle keyer conversion, the CW engine conversion, and the freeze harness.
- **Code review: not run.** The shipping gate in `ce-work` requires it before any PR. The diff is behaviour-bearing, so the mechanical-diff skip does not apply.
- **Nothing pushed.** `gh` in this checkout defaults to upstream, so any issue or PR command needs `-R iu3qez/deskhpsdr`.

## Decisions and their owners

The user decided, with the alternatives in view:

- Decouple keying from the interface thread first, rather than hunting the stall.
- When transmit is not ready, hold the element rather than drop it: a late character beats a truncated one.
- Both directions move, not only receive-to-transmit, because a stalled release leaves the transmitter keyed.
- Only CW break-in moves to the new path; the MOX button, PTT, VOX, TUNE and the CAT/TCI MOX commands keep the existing route.
- The `DIAG` instrumentation gets its own commit, and this session stops after U1 and U2.

My own calls, not the user's, which the next agent may revisit:

- The capture and replay abort belongs in the RF half. In the converger a short CW element would collapse the pair and the abort would never run.
- `rxtx()` queues the converger instead of calling it. Its callers assign `mox`/`vox`/`tune` only after it returns, and the converger reads that state rather than taking an argument. The visible consequence is that panels move one main-loop iteration later than before.
- Both units ran inline rather than through subagents, because I held the verified line-by-line classification and a cold worker would have paid ramp-up on exactly the file that matters.

## Fragile local state, all machine-local

- `/Users/sf/Developer/deskhpsdr/.claude/worktrees/confident-hopper-0653a7` - this worktree. `wdsp-libs` inside it is an untracked symlink to the primary checkout and must never be committed.
- `/Users/sf/Developer/deskhpsdr/.claude/worktrees/baseline-pre-rtmidi` - a detached worktree at `05ae34aa` with its own build, used to prove the fault predates the 2026-09-18 merge. It also carries the same uncommitted `DIAG` patch. Delete it when the comparison is no longer wanted; nothing else depends on it.
- **Build only with `make -j8 deskhpsdr`.** A bare `make` runs `update_libs.sh`, which upgrades every Homebrew package on the machine and deletes the previous versions.
- `~/Library/Application Support/deskHPSDR/deskhpsdr.log*` - the measurement evidence lives in these rotated logs. Every application start rotates them and only five generations are kept, so a handful of launches will erase the numbers quoted above. Copy them elsewhere if they still matter.

## Verification performed, and what is still missing

Performed: build, `cppcheck`, the two existing test harnesses, and a manual read of the split for RF/GUI separation. Nothing failed.

Missing, and it is the real gate for U1 and U2: run the binary and exercise MOX on and off in SSB, tune on and off, then MOX again straight after. Receiver panels should leave and return, the transmit panel should appear, and stderr should show no GTK warning. With the transmit level window enabled it should appear in SSB and stay away in CW. There is no automated coverage for any of this: `make test` only runs `tci-spectrum-test` and `property-test`, neither of which touches `radio.c`, `iambic.c` or `cw_engine.c`.

## Where this can go next

One sequential path, in this order:

1. Verify U1 and U2 on the air as described above. If the panels misbehave, suspect the deferred converger first.
2. Implement U3 to U6 from the plan. U3 carries the delicate parts: the three-outcome result contract, the bounded mutex acquisition, and the refusal predicates that need new accessors in `src/tci.c` and `src/voice_keyer.c`.
3. Run the code-review gate, then decide about a PR.

`ce-work` resumes the plan directly; `ce-code-review` covers step 3. The plan's Definition of Done requires the `DIAG` lines and the freeze harness to be removed before the work is declared finished, while the permanent fallback log line stays.

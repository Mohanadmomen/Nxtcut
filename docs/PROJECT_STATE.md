# NxtCut - Project State

Single place to see what NxtCut is, what is done, what is left, and how to work on it.
Read this first. The full plan (every step, UI steps, milestones) is in `docs/ROADMAP.md`.
Update this file after every merged step (the "Status" and "Next" sections).

Last updated: 2026-10-10 (Step 4C-1 built, 560 tests expected; merge pending green CI).

## What NxtCut is
A professional multi-track desktop video editor: C++20, Qt 6 (Widgets) for the UI, MIT licensed.
Goal: match the features of FreeCut (https://github.com/walterlow/freecut, a browser editor) while being
native, offline, fast and extensible with plugins. Platforms: Windows, Linux, macOS.
Repository: https://github.com/Mohanadmomen/Nxtcut

Priorities, in order: (1) clean, modular, tested code; (2) correctness (frame-exact editing, no A/V
drift, no data loss); (3) performance; (4) feature count.

## Status

| Step | Scope | State |
|---|---|---|
| 0 | Foundation: CMake, vcpkg, GoogleTest, warnings, sanitizers, architecture check, CI | done |
| 1, 1B, 1C | `core`: errors, time, frame rate, timecode, uuid, logging, thread pool, geometry, color | done |
| 2 | `model`: project, sequence, track, clip, validation, compound-clip graph | done |
| 3A | `commands`: ChangeSet, History, Editor, transactions, 15 simple commands | done |
| 3B | Timeline edits: add, insert, overwrite, move, delete, split, trim, ripple delete, close gap, join, link, unlink | done (merged) |
| 3C-1 | Advanced edits (1): roll, slip, slide; unify source-offset rounding of ripple/non-ripple trims | done (merged) |
| 3C-2 | Advanced edits (2): rate-stretch (speed 1/100x..100x), track push/pull (`ShiftTrackClips`) | done (merged) |
| 4A | `keyframes` module: easing, cubic bezier, keyframe track, fast evaluation (core-only, no model changes) | done (merged) |
| 4B | Wire keyframes into `Property<T>`; `model` may depend on `keyframes`; `identical` (no new validation rules) | done (merged) |
| 4C-1 | Keyframes in timeline edits: `model/clip_animation.hpp` (`for_each_animatable_property`, `shift_keyframes`, `scale_keyframes`); split, head trims, roll, slide, overwrite, insert, rate-stretch, join keep keyframed motion attached (rules R1-R5 in `docs/COMMANDS.md`) | done (pending merge) |
| 4C-2 | Keyframe commands (set, remove, move, set interpolation, clear) with `model::PropertyRef`; then spring and motion modifiers | NEXT |
| 5 | Storage: JSON project files, schema versions, autosave | todo |
| 6 | Media layer: FFmpeg probe/decode/seek, thumbnails, waveforms | todo |
| 7 | Playback: clock, decode-ahead, frame cache, scrubbing | todo |
| 8, 8b | CPU render core, text and vector | todo |
| 9 | Effects and transitions | todo |
| 10 | Audio engine | todo |
| 11 | Export | todo |
| 12 | GPU backend | todo |
| 13 | Plugins (native API, OpenFX, VST3/CLAP) | todo |
| 14, 15 | Analysis, AI features (optional) | todo |

Milestone M1 = headless engine that loads a project and exports a video (steps up to 11 + a small CLI).
After M1: the Qt UI (app steps A0 to A14).

## Architecture in one page
- `engine/` is Qt-free. `app/` (later) depends on `engine/`, never the reverse.
- Module dependencies flow one way and are enforced by `scripts/check_architecture.py`:
  core <- model <- commands; keyframes, storage, media, playback, render, effects, audio, export,
  plugins, analysis are added later (the allowed-dependency table is in `docs/ARCHITECTURE.md`).
- Time: integer ticks, 705,600,000 per second. Errors: `core::Result<T>`, no exceptions for expected failures.
- Edits: every command computes a `ChangeSet` (before/after entries); apply/undo are generic.
  The `Editor` publishes immutable `shared_ptr<const Project>` snapshots; readers never lock.
- Project files: JSON with schema versions and migrations (Step 5).
- Details: `docs/ARCHITECTURE.md`, `docs/MODEL.md`, `docs/COMMANDS.md`, `docs/CODING_STANDARDS.md`.

## Decisions already made (do not reopen without a reason)
Speed matters: hot paths (property evaluation, time conversion, rendering inputs) are allocation-free, `noexcept`
where possible, and have documented complexity; correctness first, then measure before micro-optimizing.
C++20 + Qt Widgets; CMake presets + vcpkg; GoogleTest; FFmpeg linked dynamically (LGPL, no GPL parts);
CPU renderer is the reference, GPU later; build order: model, commands, keyframes, storage, then media;
timeline edits follow mainstream editors (sync-lock ripple, frame snapping, linked clips edit together);
no Adobe plugin loading (proprietary), OpenFX and VST3/CLAP instead.

## Next
Step 4C-2 on a new branch (suggested `step-4c2-keyframe-commands`): keyframe COMMANDS in `commands`. Already true
after 4C-1: every timeline edit keeps keyframes attached (split = shift-only, head edits shift by -delta,
rate-stretch scales, join is the exact inverse of split; see `docs/COMMANDS.md`, Step 4C-1) and
`model::for_each_animatable_property` is the single place that knows every animatable property of a clip.
Defaults decided for 4C-2 (the maintainer said "go"):
(1) `model::PropertyRef`, one small value type in `model` (transform field, audio volume, text font size,
text color, effect id + param name) with one resolver used by all keyframe commands and by the visitor's
ordering; effect params are included although no AddEffect command exists yet (tests build effects through the
model); (2) commands `SetKeyframe` (insert or replace), `RemoveKeyframe`, `MoveKeyframe`,
`SetKeyframeInterpolation`, `ClearKeyframes`, all through the existing ChangeSet machinery; edits copy the track,
change the copy, call `set_keyframes` (never mutate a shared track); (3) rules: set/move snap the time to the frame
grid and require `0 <= time <= clip.duration`; value type must match the property (double vs Color); ranges as for
constants (opacity and crop in [0,1], volume >= 0, font size > 0, others finite); wrong clip kind (for example
transform on an audio clip) gives InvalidArgument "not applicable"; unknown keyframe gives NotFound; locked track
gives "locked"; `MoveKeyframe` onto an occupied time fails; no-ops give an empty ChangeSet; linked partners are
never touched; (4) `SetClipProperties` also range-checks the keyframe values inside a supplied transform's tracks.
Spring easing and motion modifiers come after 4C-2.

## Known technical debt
- `diff_to_changes` and clip lookups are linear (fine now; benchmark at Steps 6-7).
- `Editor::commit` copies the project and runs a full validate (benchmark later).
- `SlipClip` with a zero delta on an image or text clip returns an empty ChangeSet instead of the "slip" error.
- `spdlog` is installed but unused. `compile_checks.hpp` exists twice (tests/core, tests/model).
- GitHub Actions versions should be bumped; add `THIRD_PARTY_NOTICES` when the first dependency is bundled.
- Keyframes outside a clip's range are kept on purpose (split is shift-only, tail trims keep them); an explicit
  "prune out-of-range keyframes" command can come later. `shift_keyframes` and `scale_keyframes` allocate (they
  rebuild tracks) and are not `noexcept`; clips without animated properties pay only pointer checks.
- `SetClipProperties` range checks (opacity, crop) look at constants only; 4C-2 adds the keyframe-value checks.
  The crop_left + crop_right <= 1 rule is not checked across animated values.
- Sub-frame keyframe times can appear after rate-stretch (times are scaled, not re-snapped); intended.

## How to build and test (Windows)
Use the "x64 Native Tools Command Prompt for VS", then from the repo root:

    scripts\verify.cmd

It configures, builds, runs all tests, runs the architecture check and the format check, and prints a
summary. Logs are in `build\` (git-ignored). Presets: `windows-debug`, `windows-release`; Linux/macOS:
`debug`, `release`, `asan-ubsan`, `tsan`.

## How work is done (workflow)
1. A step is designed first (scope, defaults, required test cases), then written as ONE prompt.
2. An AI coding agent implements it on a branch `step-N-name` (rules for the agent: `AGENTS.md`).
3. The developer runs `scripts\verify.cmd`; the code is reviewed (public headers, ownership, locking,
   link and lock handling) before commit.
4. Format with clang-format 18.1.8, commit, push, open a pull request into `main`.
5. CI must be green on all 7 jobs (lint + architecture, Ubuntu GCC 13, Ubuntu Clang 17, Windows MSVC,
   macOS Apple Clang, ASan+UBSan, TSan). Merge, sync `main`, delete the branch.

## Picking up a task (for a new contributor)
1. Read this file, `AGENTS.md`, `docs/ARCHITECTURE.md` and `docs/CODING_STANDARDS.md`.
2. Take the NEXT step in the Status table; open an issue or message the maintainer before starting.
3. One branch and one pull request per step. Keep steps small enough to review.
4. Every public behaviour needs tests; every command goes through the `round_trip` test helper.

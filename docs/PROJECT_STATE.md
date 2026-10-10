# NxtCut - Project State

Single place to see what NxtCut is, what is done, what is left, and how to work on it.
Read this first. The full plan (every step, UI steps, milestones) is in `docs/ROADMAP.md`.
Update this file after every merged step (the "Status" and "Next" sections).

Last updated: 2026-10-10 (Step 4B merged, 541 tests, CI green).

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
| 4C | Keyframe commands (add, move, delete, set interpolation) and keyframe handling on trims; then spring and motion modifiers | NEXT |
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
Step 4C on a new branch (suggested `step-4c-keyframe-commands`): keyframe commands in `commands`. Already true
after 4B: `Property<double>` and `Property<core::Color>` can hold a shared immutable `KeyframeTrack<T>`
(`set_keyframes`, `clear_keyframes`, `keyframes()`, `value_at(ClipTime)`, `value_at(ClipTime, Cursor&)`);
keyframe times are clip-relative ticks and may lie outside the clip; the model validator has no range rules for
property values, so keyframes need no validation; edits copy the track, change the copy, and call
`set_keyframes` (never mutate a shared track). Open design questions to ask the user first (recommended
default in brackets): (1) how a command addresses a property: a `PropertyRef` naming clip + transform field,
audio volume, text size/color, or effect id + param name [one small value type, resolved by a single helper
used by all keyframe commands]; (2) command set [`SetKeyframe` (insert or replace), `RemoveKeyframe`,
`MoveKeyframe`, `SetKeyframeInterpolation`, `ClearKeyframes`, all undoable via the existing ChangeSet
machinery]; (3) what happens to keyframes on head trim, split, join, slip, rate-stretch [keep clip-relative times
unchanged on tail trims; on head trims shift keyframes by the head delta so motion stays attached to the
media; split copies the relevant keyframes to both halves with an interpolated boundary keyframe; rate-stretch
scales keyframe times by the duration ratio; join concatenates; each rule gets its own tests]; (4) whether to
split 4C into 4C-1 (commands) and 4C-2 (trim/split/join/stretch handling) [yes, two steps]. Spring easing and
motion modifiers come after 4C.

## Known technical debt
- `diff_to_changes` and clip lookups are linear (fine now; benchmark at Steps 6-7).
- `Editor::commit` copies the project and runs a full validate (benchmark later).
- `SlipClip` with a zero delta on an image or text clip returns an empty ChangeSet instead of the "slip" error.
- `spdlog` is installed but unused. `compile_checks.hpp` exists twice (tests/core, tests/model).
- GitHub Actions versions should be bumped; add `THIRD_PARTY_NOTICES` when the first dependency is bundled.

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

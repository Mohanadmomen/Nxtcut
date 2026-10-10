# NxtCut Document Commands (`commands` Module)

## Overview

The `commands` module (`nxtcut::commands`) provides the transactional mutation engine for NxtCut project documents. It is the **sole authorized subsystem** permitted to modify a `model::Project`.

All user-initiated editing actions, programmatic batch scripts, and automation workflows flow through transactional command execution.

---

## The ChangeSet Design

### Rationale: Why No Per-Command Undo Code

In traditional command pattern implementations, each command class must provide both `execute()` and a hand-crafted `unexecute()` method. In complex multi-track timeline editors, this pattern has severe drawbacks:
1. **Asymmetry and Drift**: Bugs frequently arise when undo logic does not perfectly mirror forward execution across complex corner cases (e.g., secondary link removals, multi-track shifts).
2. **Exponential Test Matrix**: Every new command requires separate, extensive test suites for both its forward execution and custom undo logic.
3. **Partial Mutation Risks**: If a command mutates document state in-place and fails halfway through, rolling back the project to its previous state is complex and error-prone.

NxtCut eliminates per-command undo logic by design:
- **Pure `build()` Functions**: Every command is an immutable value struct providing a pure `build(const Project&, core::UuidGenerator&)` function that calculates a `ChangeSet` without modifying the project.
- **Generic `apply()` and `inverse()`**: A `ChangeSet` is an ordered sequence of fine-grained `Change` variant records capturing `before` and `after` states. Generic `apply(project, change_set)` executes mutations in forward order, while `inverse(change_set)` reverses the sequence and swaps `before` and `after` states.
- **Exact Undo by Construction**: Undo and redo are guaranteed to be exact by construction and tested comprehensively in one centralized engine (`apply.cpp`).

---

## Granular Change Records

A `ChangeSet` aggregates an ordered list of `Change` variant alternatives:

| Change Record | Description | Lifecycle Semantics |
| :--- | :--- | :--- |
| `ProjectPropertiesChange` | Name and main sequence changes | Both sides present |
| `MediaChange` | Media asset addition, removal, or modification | Add: `before` empty; Remove: `after` empty; Modify: both present |
| `SequenceChange` | Sequence creation or deletion | Exactly one side present |
| `SequenceSettingsChange` | Frame rate, canvas, sample rate, background | Both sides present |
| `TrackChange` | Track addition or deletion with slot index | Exactly one side present |
| `TrackMove` | Track reordering within a sequence | `from_index` and `to_index` swapped on inverse |
| `TrackPropertiesChange` | Track name, enabled, locked flags | Both sides present |
| `ClipChange` | Clip creation, deletion, or property changes | Add: `before` empty; Remove: `after` empty; Modify: both present |
| `MarkerChange` | Timeline marker addition or removal | Add: `before` empty; Remove: `after` empty |

---

## The Canonical-Order Invariant

To guarantee deterministic document structure and linear scrubbing searches, `apply()` strictly maintains the canonical sort invariant:
1. **Track Clips**: Ordered by ascending timeline `start` ticks. Ties are broken deterministically by `id` (UUID byte ordering).
2. **Sequence Markers**: Ordered by ascending timeline `time` ticks. Ties are broken deterministically by `id`.

`ClipChange` and `MarkerChange` insertions use `std::lower_bound` on the canonical ordering comparator to maintain sorted order in $\mathcal{O}(N)$ insertion time. In addition, `normalize(Project&)` is available to re-establish canonical order across existing tracks and sequences using a stable sort.

---

## Command Conventions & Error Handling

All commands adhere to standardized error reporting:
- **`ErrorCode::NotFound`**: Returned when a referenced sequence, track, clip, marker, or media asset cannot be found.
- **`ErrorCode::InvalidArgument`**: Returned for invalid parameter values, zero canvas dimensions, negative durations/times, all-optional-empty calls, attempts to modify locked tracks (error message contains `"locked"`), or attempts to delete entities in use (error message contains `"in use"`).
- **`ErrorCode::OutOfRange`**: Returned when an insertion or move index exceeds allowable track bounds.
- **`ErrorCode::AlreadyExists`**: Returned when attempting to insert an entity with an existing ID.
- **No-Op Edits**: If an edit would result in identical property values, `build()` returns an empty `ChangeSet`. Committing an empty ChangeSet is a successful no-op that publishes no history entry and invokes no listeners.

---

## Step 3A Commands Reference (15 Commands)

| # | Command | Arguments | Business & Validation Rules |
| :--- | :--- | :--- | :--- |
| 1 | `AddTrack` | `sequence`, `kind`, `name`, `index` (optional) | Inserts new track at `index` (default: end). Out-of-bounds index yields `OutOfRange`. Generates new `TrackId`. |
| 2 | `RemoveTrack` | `sequence`, `track` | Refused if track is locked (`InvalidArgument` with `"locked"`). Cleans up clip links: if fewer than 2 clips with a given `LinkId` remain outside the removed track, their `link_id` is set to `nullopt`. |
| 3 | `MoveTrack` | `sequence`, `track`, `new_index` | Reorders track within sequence. Allowed on locked tracks. Out-of-bounds `new_index` yields `OutOfRange`. Same index returns empty `ChangeSet`. |
| 4 | `SetTrackProperties` | `sequence`, `track`, `name`, `enabled`, `locked` | Allowed on locked tracks (enables unlocking). All optionals empty yields `InvalidArgument`. Unchanged values yield empty `ChangeSet`. |
| 5 | `AddMedia` | `asset` | Nil `id` generates a new `MediaId`. Duplicate ID yields `AlreadyExists`. Empty path yields `InvalidArgument`. Video/Audio assets require valid stream info and duration $> 0$. |
| 6 | `RemoveMedia` | `media` | Refused with `InvalidArgument` containing `"in use"` if referenced by any clip in any sequence. |
| 7 | `RelinkMedia` | `media`, `new_path` | Empty path yields `InvalidArgument`. Same path returns empty `ChangeSet`. |
| 8 | `SetClipProperties` | `sequence`, `clip`, `name`, `enabled`, `blend_mode`, `transform` | Refused on locked tracks (`"locked"`). Validates `TransformProps`: opacity $\in [0, 1]$, crop margins $\in [0, 1]$, $\text{crop\_left} + \text{crop\_right} \le 1$, $\text{crop\_top} + \text{crop\_bottom} \le 1$. |
| 9 | `SetAudioClipProperties`| `sequence`, `clip`, `volume`, `fade_in`, `fade_out` | Clip must hold `AudioContent`. Volume $\ge 0$. Fades $\ge 0$ and $\text{fade\_in} + \text{fade\_out} \le \text{clip.duration}$ (checked arithmetic). Refused on locked tracks. |
| 10 | `AddMarker` | `sequence`, `time`, `text`, `color` | Time must be $\ge 0$. Generates new `MarkerId`. Inserted in canonical sorted order. |
| 11 | `RemoveMarker` | `sequence`, `marker` | Removes marker from sequence. Unknown marker yields `NotFound`. |
| 12 | `CreateSequence` | `name`, `frame_rate`, `canvas`, `sample_rate`, `background` | Canvas width and height must be $> 0$. Generates new `SequenceId`. Initializes sequence with zero tracks. |
| 13 | `RemoveSequence` | `sequence` | Refused with `InvalidArgument` if sequence is `main_sequence`. Refused with `"in use"` if referenced by any compound clip. |
| 14 | `SetSequenceSettings` | `sequence`, `name`, `frame_rate`, `canvas`, `sample_rate`, `background` | Canvas dimensions must be $> 0$. All optionals empty yields `InvalidArgument`. Unchanged values yield empty `ChangeSet`. |
| 15 | `SetProjectProperties` | `name`, `main_sequence` | If specified, `main_sequence` must exist in project sequences (else `NotFound`). All optionals empty yields `InvalidArgument`. |

---

## Id generation

Commands draw ids from the editor's injected generator; the generator given to an Editor must not replay the id sequence that produced the project's existing ids (production uses a randomly seeded generator; tests use ONE generator for the fixture and the editor).

---

## Transactions

The `Transaction` class provides move-only RAII scope for grouping multiple command executions into a single atomic undo/redo step:
- **Working Document State**: Commands execute against a local copy of the project (`Transaction::project()`).
- **Failure Isolation**: A failed command build or apply in a transaction leaves the transaction's working state unaltered.
- **Automatic Rollback**: If a transaction is destroyed without calling `commit()`, all changes are discarded and the editor state lock is released.
- **Single Open Transaction Policy**: Only one transaction may be open at any time. Nested or concurrent transaction attempts return `ErrorCode::InvalidArgument` (`"transaction open"`).
- **Transaction Misuse Guards**: A transaction is open only until `commit()`, `rollback()`, a failed `commit()`, or a move-from. Calling `execute()` or `commit()` on a transaction that is not open returns `ErrorCode::InvalidArgument` (`"transaction is not open"`). Calling `rollback()` on a finished transaction is a harmless no-op.
- **Failed Commit Ends Transaction**: A failed `commit()` (validation rejected) ends the transaction: the working project is discarded and the editor accepts new transactions again.

---

## Validation on Commit

When `EditorOptions::validate_on_commit` is enabled (default `true`):
- Upon `commit()`, the full document validation suite (`model::validate`) executes against the final working project.
- If validation finds any structural or semantic issues, the commit is rejected, returning the validation error augmented with context `"edit rejected"`. The rejected transaction ends immediately, discarding the working project state and releasing the editor to accept new transactions.
- The project snapshot, undo history, and listeners remain completely untouched on rejection.

---

## Snapshots and Threading Model

- **Editor Thread Affinity**: The `Editor` class is strictly **UI-thread only**. All mutations, transaction lifecycles, and history traversals must execute on the main thread.
- **Immutable Snapshots**: `editor.snapshot()` returns a `std::shared_ptr<const model::Project>`. Once published, snapshots are completely immutable and safe for concurrent read access by background rendering pipelines, waveform generators, and export encoders without mutex locking.

---

## EditListener Contract

`EditListener` provides synchronous notifications for document mutations:
- **Synchronous Notification**: Listeners are notified immediately after a new snapshot is published, before the mutation method returns.
- **Registration Order**: Listeners execute in the exact order they were registered.
- **Event Filtering**: Listeners are called only for actual document state updates (`Commit`, `Undo`, `Redo`). Rolled-back transactions, failed command builds, empty ChangeSets, and rejected commits emit no events.
- **Re-Entrancy Guard**: Listeners are forbidden from re-entrantly calling `execute()`, `begin_transaction()`, `undo()`, or `redo()`. Attempting re-entrant execution returns `ErrorCode::InvalidArgument` with message `"re-entrant"`, while the outer commit completes safely.

---
## History, Undo and Redo

- The history keeps up to `EditorOptions::max_history_steps` undo steps (default 200). When the limit is reached the oldest step is dropped. A new commit clears the redo stack.
- `undo()` and `redo()` apply the stored (or inverted) ChangeSet to a copy of the current project and publish a new snapshot. They do NOT run validation, because both states were validated when they were committed.
- `undo_label()` and `redo_label()` return the label of the next step (for example "Add Track"), or nothing when no step is available.
- Every successful `execute()` or `commit()` returns an `EditReceipt` listing the ids of the entities the edit created, so a UI can select them.
- A ChangeSet stores full copies of the clips it changes, so memory use grows with the size of the edit, not with the size of the project.

---
## Step 3B Timeline Editing Commands

Step 3B introduces 12 timeline geometry editing commands. All timeline edits operate transactionally through `ChangeSet`s of `ClipChange` records without per-command undo code.

### General Design & Conventions

#### 1. Frame Snapping Rule
All timeline times (`TimelineTime`) and durations (`core::Duration`) supplied to a timeline command are snapped to the sequence's discrete frame grid using `core::snap_to_frame(..., sequence.frame_rate, core::RoundingMode::Nearest)`.
- A supplied duration that snaps to less than one frame (i.e. `< frame_duration(sequence.frame_rate)`) is rejected with `ErrorCode::InvalidArgument`.
- Edits snap to the nearest frame boundary to guarantee zero accumulated subframe drift.

#### 2. Ripple Scopes
- **`RippleScope::AllUnlockedTracks`** (Default): Shifts all unlocked tracks across the sequence (sync-lock behavior matching industry standard NLEs). Locked tracks are excluded from the ripple scope and are untouched; their presence outside the ripple scope is not an error.
- **`RippleScope::EditedTracksOnly`**: Restricts ripple shifts solely to the tracks containing the edited or placed clips (including tracks of linked partners that were affected). If any track in the scope (including an included partner track) is locked, the command fails immediately with `ErrorCode::InvalidArgument` containing `"locked"`.

#### 3. Link Coordination & Link Repair
- **Default Grouping**: Linked clips (sharing the same `LinkId` within the sequence) are edited together by default unless `ignore_links == true`.
- **Placement Coordination**: When adding, inserting, or overwriting multiple `PlacedClip` items, all new clips share ONE fresh `LinkId` (the video-plus-audio pair scenario). When placing exactly one clip, `link_id` is cleared (`nullopt`).
- **Split Link Rule**: In a split operation, left halves retain their original `LinkId`. Right halves originating from the same link group share ONE newly generated `LinkId` in deterministic first-seen order if there are at least 2 right halves; otherwise the single right half has no link (`nullopt`).
- **Link Repair**: After any timeline modification, if fewer than 2 clips with a given touched `LinkId` remain in the sequence, the surviving clip's `link_id` is set to `nullopt` (identical to the invariant maintained by `RemoveTrack`). If clearing a survivor's link would mutate a clip on a locked track, the entire command fails immediately with `ErrorCode::InvalidArgument` containing `"locked"`.

#### 4. Overlap & Timeline Invariants
- **No Silent Overlaps**: On every touched track, no clip may start before the latest end time of clips preceding it in start order (`[t_1, t_2)` and `[t_2, t_3)` touching boundaries are permitted). Overlaps result in `ErrorCode::InvalidArgument` containing `"overlap"`.
- **Non-Negative Start**: Every clip's start time must be non-negative (`>= 0`).
- **Checked Arithmetic**: All time math operations use checked arithmetic (`model::detail::checked_add` and `checked_sub`). Arithmetic overflow returns `ErrorCode::Overflow`. Overflow of any time computation, including from user-supplied times (such as extreme timeline positions, trim edges, or durations), returns an error and never throws.
- **Audio Fade Fitting**: Whenever an audio clip's duration shrinks or a clip is split (in `split_clip`, `trim_head`, `trim_tail`, ripple head trims, or overwrite strictly-contains splits), audio fades are fitted using `fit_audio_fades`: if `fade_in > clip.duration`, `fade_in = clip.duration`; then if `fade_out > clip.duration - fade_in`, `fade_out = clip.duration - fade_in` (so the two fades never overlap).
- **Source Range Bounds**: Every created or modified clip must respect source bounds: `source_in >= 0` and `source_in + source_span <= media.duration` (or nested sequence duration for compound clips). Image and text clips require `source_in == 0` and `speed == 1/1`. Violations return `ErrorCode::InvalidArgument`.
- **Compound Cycles**: Placing or modifying a compound clip checks `model::would_create_cycle`; cycles are rejected with `ErrorCode::InvalidArgument`.
- **Track Kind Compatibility**: Video tracks admit Video, Image, Text, and Compound clips. Audio tracks admit only Audio clips. Mismatches return `ErrorCode::InvalidArgument`.

#### 5. Locked Track Enforcement
Locked tracks are enforced strictly within command `build()` methods:
- If ANY track directly modified, included by link expansion, or designated as an edit destination is locked, the command fails immediately with `ErrorCode::InvalidArgument` containing `"locked"`.
- In `InsertClips`, after the scope tracks are computed (including partner tracks added under `EditedTracksOnly`), if ANY track in the scope is locked, the command fails immediately with `ErrorCode::InvalidArgument` containing `"locked"`.
- If link repair attempts to clear a link on a surviving clip that resides on a locked track, the entire command fails immediately with `ErrorCode::InvalidArgument` containing `"locked"`.
- Locked tracks outside the ripple scope remain completely untouched and do not cause failure.

#### 6. Cross-Track Moves and EditReceipts
When a clip moves to another track, it is recorded as a removal on the source track (`before` present, `after` empty) and an addition on the destination track (`before` empty, `after` present) with the same `ClipId`. Because `receipt_of` collects all `ClipChange` records where `!change.before.has_value() && change.after.has_value()`, the moved clip's `ClipId` appears in `EditReceipt::created_clips`.

#### 7. Thread Safety
All timeline commands are immutable plain data structs. `build(const Project&, core::UuidGenerator&)` is `const`, pure, and re-entrant.

---

### Part 1 Commands Reference

| Command | Arguments | Business & Validation Rules |
| :--- | :--- | :--- |
| `AddClips` | `sequence`, `at`, `clips` (`vector<PlacedClip>`) | Places clips at `snap(at)` without moving existing material. Fails with `"overlap"` if any placed clip collides with existing clips. Single placed clip has no link; multiple placed clips share ONE fresh `LinkId`. Destination tracks must exist, be unlocked, and match clip content kinds. Receipt lists created clip IDs. |
| `InsertClips` | `sequence`, `at`, `clips`, `scope` | Insert edit. `delta` = maximum snapped duration among `clips`. On all scope tracks (destination tracks always in scope): clips straddling `at` (`start < at < end`) are split at `at`; clips with `start >= at` shift later by `delta`. New clips are inserted at `at`. Inserting at clip boundary splits nothing. If ANY track in scope is locked (including partner tracks added under `EditedTracksOnly`), fails with `"locked"`; locked non-destination track outside scope is untouched. |
| `OverwriteClips` | `sequence`, `at`, `clips` | Places clips at `snap(at)` and removes covered material on destination tracks only. Duplicate destination tracks in `clips` are rejected with `ErrorCode::InvalidArgument` containing `"duplicate"`. Fully covered clips are removed; clips covered at head or tail are trimmed; clips strictly containing the placed range are split into two with the middle excised (right half gets a fresh `ClipId`). Overwriting empty space acts as `AddClips`. Audio fades are fitted on split halves (`fit_audio_fades`). Link repair applies (failing with `"locked"` if a survivor is on a locked track). Destination tracks must be unlocked. |
| `MoveClips` | `sequence`, `moves` (`vector<ClipMove>`), `ignore_links` | Moves clips to `snap(new_start)` and/or `new_track` preserving duration. Unless `ignore_links`, linked partners move by the same delta on their own tracks. Moving clips are detached from scratch tracks before reinsertion (enabling swap moves of equal duration). Overlap check runs on the final state. Move changing nothing returns empty `ChangeSet`. Source/destination tracks must be unlocked. |
| `DeleteClips` | `sequence`, `clip_ids`, `ignore_links` | Lift delete: removes clips (and linked partners unless `ignore_links`), leaving gaps. Subsequent clips do not shift. Duplicate IDs are de-duplicated. Link repair applies. Locked track of any deleted clip or affected partner fails with `"locked"`. Unknown clip returns `NotFound`. |
| `SplitClip` | `sequence`, `clip`, `at`, `ignore_links` | Splits clip at `snap(at)` (`start < at < end` strictly inside). Unless `ignore_links`, linked partners containing `at` are split on their tracks at the same time. Left halves keep original `ClipId` and `LinkId`; right halves share ONE new `LinkId` in deterministic first-seen order if $\ge 2$ right halves originate from the same group. Audio fade-out becomes 0 on left; fade-in becomes 0 on right; fades are fitted via `fit_audio_fades`. Right halves appear in `created_clips`. |

---

### Part 2 Commands Reference

| Command | Arguments | Business & Validation Rules |
| :--- | :--- | :--- |
| `TrimClip` | `sequence`, `clip`, `edge`, `new_edge`, `ripple`, `scope`, `ignore_links` | Moves edge to `snap(new_edge)` leaving at least 1 frame. Non-ripple: trimmed edge moves without shifting other clips; fails with `"overlap"` if extended into another clip. Ripple: head trim keeps original `start` with `source_in` advanced; clips starting at or after the edited clip's original end shift by the duration delta across scope tracks. Linked partners move by matching delta unless `ignore_links`. Audio fades are fitted via `fit_audio_fades`. Source bounds enforced. No-op returns empty `ChangeSet`. |
| `RippleDeleteClips` | `sequence`, `clip_ids`, `scope`, `ignore_links` | Deletes clips (and linked partners unless `ignore_links`), then closes removed time. Deleted ranges across tracks merge into disjoint intervals $[A_i, B_i)$. Remaining clips on scope tracks with `start >= B_i` shift earlier by cumulative closed lengths. Clips overlapping an interval do not shift. If shift causes overlap, fails with `"overlap"`. Deleted clip on locked track fails with `"locked"`. |
| `CloseGap` | `sequence`, `track`, `at`, `scope` | `snap(at)` must lie in an empty gap on `track` preceding a subsequent clip. Fails if `at` is inside a clip or no clip follows. Shifts all clips starting at or after the gap's end earlier by the gap length across scope tracks. Fails with `"overlap"` if collision occurs on any track. Track must be unlocked. |
| `JoinClips` | `sequence`, `pairs` (`vector<JoinPair>`) | Recombines adjacent split halves on the same track. Requires matching media, identical speed, contiguous source (`second.source_in == first.end_source`), and identical properties. Restores `first`'s `ClipId` with combined duration; for audio, restores `first.fade_in` and `second.fade_out`. Image clips bypass source checks. Text/compound clips rejected. Link repair applies. |
| `LinkClips` | `sequence`, `clip_ids` | Groups $\ge 2$ unlinked clips with ONE fresh `LinkId`. Fails with `"already linked"` if any specified clip currently holds a link. Tracks of all affected clips must be unlocked. |
| `UnlinkClips` | `sequence`, `clip_ids`, `ignore_links` | Clears link associations. Unless `ignore_links`, clears the entire link group of each named clip; with `ignore_links`, clears only named clips and link repair removes orphan links (< 2 members). Unlinking unlinked clips returns an empty `ChangeSet`. Locked tracks of affected clips fail with `"locked"`. |

---

## Step 3C-1 Advanced Trimming Commands

Step 3C-1 introduces advanced trimming modes: `RollEdit`, `SlipClip`, and `SlideClip`. All three operate transactionally via `ChangeSet`, preserve the total timeline duration of the edited region, and do not create or delete clips (`created_clips` is empty).

### General Design & Conventions

#### 1. Source-Offset Rounding
When converting a timeline delta to source media ticks across rational playback speeds, the calculation is evaluated as:
$$\text{sign}(\Delta) \cdot \lfloor |\Delta| \cdot \frac{\text{numerator}}{\text{denominator}} \rfloor$$
Rounding is strictly toward zero (`core::RoundingMode::Floor` on $|\Delta|$ followed by negation for negative deltas). This rule is implemented in a single unified helper (`detail::source_offset_for_delta`) and shared by head trims, roll edits, slip edits, and slide edits to guarantee reversible symmetric rounding and zero accumulated subframe drift.

#### 2. Media Handle Limits
Media boundaries and source limits are treated as hard barriers rather than clamps: an edit requiring source ticks before zero or beyond the media duration fails immediately with `ErrorCode::InvalidArgument`. Image and Text clips have no source handle limits (their `source_in` remains zero).

#### 3. Locked Track and Link Invariants
- If any track containing a primary edited clip, linked partner, or neighbor is locked, the entire command fails with `ErrorCode::InvalidArgument` containing `"locked"`.
- Linked clips are coordinated together by default unless `ignore_links == true`.
- No clips are added or removed; therefore, `repair_links()` is not needed.

---

### Part 3 Commands Reference

| Command | Arguments | Business & Validation Rules |
| :--- | :--- | :--- |
| `RollEdit` | `sequence`, `left`, `right`, `new_edit`, `ignore_links` | Rolls the edit point between adjacent touching clips on the same track to `snap(new_edit)`. Left clip's tail and right clip's head move by `delta = snapped - end_of(left)`. Non-adjacent clips or clips on different tracks fail with `"adjacent"`. Left and right clips sharing the same `LinkId` fail with `"same link"` unless `ignore_links`. Both clips (and affected partners) must maintain at least 1 frame of duration (failing with `"one frame"`). Audio fades are fitted via `fit_audio_fades`. Locked tracks fail with `"locked"`. Zero delta returns empty `ChangeSet`. |
| `SlipClip` | `sequence`, `clip`, `delta`, `ignore_links` | Slips source media by `snap_delta(delta)` without changing timeline start or duration. Positive delta shifts to later source content. Image and Text clips cannot be slipped (fail with `"slip"`). Unless `ignore_links`, linked partners slip by the same timeline delta using their respective playback speeds. Source handle limits enforced via `validate_clip_media_and_source`. Locked tracks fail with `"locked"`. Zero delta returns empty `ChangeSet`. |
| `SlideClip` | `sequence`, `clip`, `new_start`, `ignore_links` | Slides clip to `snap(new_start)`. Slid clip retains duration and source offset. Left neighbor's tail and right neighbor's head roll by `delta = snapped - start`. Slid clip and linked partners must each have touching neighbors on both sides (failing with `"neighbor"`). Ambiguous clip roles (clip participating in multiple roles or touching linked clips both slid) fail with `"ambiguous"`. Neighbors must retain at least 1 frame of duration (failing with `"one frame"`). Locked tracks fail with `"locked"`. Zero delta returns empty `ChangeSet`. |

---

## Step 3C-2 Rate Stretch and Track Shifting

Step 3C-2 introduces timeline rate manipulation and track push/pull operations: `RateStretch` and `ShiftTrackClips`. Both operate transactionally via `ChangeSet`, do not add or remove clips (`created_clips` is empty), and enforce locked-track boundaries and link coordination.

### General Design & Conventions

#### 1. Fixed Source Range Rule & Speed Limits
In `RateStretch`, dragging an edge alters playback speed such that the clip's current source span ($S = \text{source\_span(clip)}$ ticks) is played across the new timeline duration ($D$ ticks). The clip's source range never changes (`source_in` is unaltered, and new speed $= \text{Speed::create}(S, D)$ ensures $\lceil D \cdot S / D \rceil = S$).
Playback speed must stay within hard boundaries:
$$\frac{1}{100} \le \frac{\text{numerator}}{\text{denominator}} \le \frac{100}{1}$$
Violations fail immediately with `ErrorCode::InvalidArgument` containing `"speed"`. Image and Text clips have no source range and cannot be rate stretched (failing with `"stretch"`). When an audio clip shrinks, audio content fades are refitted via `fit_audio_fades`.

#### 2. Track Shift Operations
`ShiftTrackClips` pushes or pulls all clips on a track starting at or after `snap(at)` by a signed duration delta. Clips starting strictly before `snap(at)` (including those straddling `at`) do not shift. Linked partners on any track shift by the same delta (even if starting before `at`) unless `ignore_links == true`. Each partner shifts at most once. Collisions with unshifted clips or partner-track clips fail with `ErrorCode::InvalidArgument` containing `"overlap"`. Negative start times fail with `"negative"`.

---

### Part 4 Commands Reference

| Command | Arguments | Business & Validation Rules |
| :--- | :--- | :--- |
| `RateStretch` | `sequence`, `clip`, `edge`, `new_edge`, `ripple`, `scope`, `ignore_links` | Changes playback speed by moving `edge` to `snap(new_edge)` while preserving the source range. Tail stretch adjusts end and duration; non-ripple head stretch sets new start while keeping end fixed; ripple head stretch keeps start, changes duration, and shifts subsequent material starting at or after the original end across scope tracks. Duration must be at least 1 frame (failing with `"one frame"`). Speed must stay within $1/100 \times \dots 100 \times$ (failing with `"speed"`). Image/Text clips rejected with `"stretch"`. Linked partners stretch identically unless `ignore_links`. Audio fades refitted via `fit_audio_fades`. Locked tracks fail with `"locked"`. Zero delta returns empty `ChangeSet`. |
| `ShiftTrackClips` | `sequence`, `track`, `at`, `delta`, `ignore_links` | Shifts all clips on `track` with `start >= snap(at)` by `snap_delta(delta)`. Unless `ignore_links`, linked partners on any track shift by the same delta once. Collision on target or partner track fails with `"overlap"`. Start before 0 fails with `"negative"`. Unknown track/sequence fails with `NotFound`. `snap(at) < 0` fails with `InvalidArgument`. Locked target or partner track fails with `"locked"`. Empty shifted set or zero delta returns empty `ChangeSet`. |

---

## Step 4C-1 Keyframes in Timeline Edits

Step 4C-1 guarantees that timeline editing commands keep keyframed motion attached to the correct media content across all geometry mutations.

### Keyframe Rules (R1–R5)

- **R1 SPLIT**: The left half keeps the clip's tracks UNCHANGED (same shared track pointers, zero copies/allocations). The right half receives copies of the animated tracks with every key time shifted by $-\text{left.duration}$. No boundary keyframes are inserted and nothing is trimmed or removed; keyframes outside the clip range remain intact. Evaluation inside each half is bit-identical to the original clip's evaluation at the same instant.
- **R2 HEAD EDITS**: Any edit that moves the head of a clip while its end remains fixed (non-ripple head trim, ripple head trim, roll of right clip, slide of right neighbor, overwrite head trim, linked partners): every key time shifts by $-\Delta$, where $\Delta = \text{new\_start} - \text{old\_start}$ (the same delta that advances `source_in`). Applies to all clip kinds (including Image and Text).
- **R3 UNCHANGED**: Tail trims, roll of left clip, slide of slid clip and left neighbor, `SlipClip`, `MoveClips`, `ShiftTrackClips`, `CloseGap`, ripple shifting of other clips: tracks are untouched (same shared pointers).
- **R4 RATE-STRETCH**: Every key time is scaled by $\frac{\text{new\_duration}}{\text{old\_duration}}$ about the clip start (always about clip start, for tail, head, and ripple head stretches) using `core::mul_div(time, new_ticks, old_ticks, Nearest)`. Values and interpolation styles are preserved. If two keys land on the same tick (or arithmetic overflows / exceeds tick bounds), the entire command fails with `ErrorCode::InvalidArgument` containing `"keyframe collision"` for collisions, or the underlying error. Nothing is dropped silently.
- **R5 JOIN**: In `JoinClips`, the "otherwise identical" comparison treats the right clip's keys as shifted by $+\text{left.duration}$. Join succeeds only if, after that shift, every animated property of the right clip is `identical()` to the left clip's (exactly reversing R1). The joined clip keeps the left clip's tracks. A mismatch (including one side animated and the other not) fails with `ErrorCode::InvalidArgument` containing `"join pair clips are not otherwise identical"`.

### Command Rule Mapping

| Command | Rules Applied | Affected Elements & Behavior |
| :--- | :--- | :--- |
| `SplitClip` | **R1** | Left half keeps shared pointers; right half shifted by $-\text{left.duration}$. |
| `InsertClips` | **R1**, **R3** | Straddling clips split via R1; shifted subsequent clips follow R3 (untouched). |
| `OverwriteClips` | **R1**, **R2**, **R3** | Strictly-contains middle excision splits via R1; head trim follows R2; tail trim follows R3. |
| `TrimClip` | **R2**, **R3** | Head trims (non-ripple and ripple) follow R2; tail trim and ripple-shifted clips follow R3. |
| `RollEdit` | **R2**, **R3** | Right clip follows R2 (head moved by $\Delta$); left clip follows R3 (tail trim). |
| `SlideClip` | **R2**, **R3** | Right neighbor follows R2 (head moved by $\Delta$); left neighbor and slid clip follow R3. |
| `RateStretch` | **R4**, **R3** | Tail, head non-ripple, and ripple head stretches follow R4; ripple-shifted clips follow R3. |
| `JoinClips` | **R5** | Second clip tested with $+\text{first.duration}$ keyframe shift; joined clip keeps first clip's tracks. |
| `SlipClip` | **R3** | Untouched (preserves timeline start, duration, and clip-relative keyframe times). |
| `MoveClips` | **R3** | Untouched (preserves duration and clip-relative keyframe times). |
| `ShiftTrackClips`| **R3** | Untouched (shifts start only; clip-relative keyframe times untouched). |
| `CloseGap` | **R3** | Untouched (shifts start only; clip-relative keyframe times untouched). |
| `RippleDeleteClips`| **R3** | Untouched (shifts start of subsequent clips only). |
| `AddClips` / `DeleteClips` | **R3** / — | Placed clips keep tracks; deleted clips removed without modifying survivors. |
| `LinkClips` / `UnlinkClips` | — | Metadata-only edits; tracks untouched. |

---
## Roadmap: Steps 3B and 3C

- **Step 3B (Implemented)**: 12 timeline geometry editing commands (`AddClips`, `InsertClips`, `OverwriteClips`, `MoveClips`, `DeleteClips`, `SplitClip`, `TrimClip`, `RippleDeleteClips`, `CloseGap`, `JoinClips`, `LinkClips`, `UnlinkClips`) with frame snapping, ripple scoping, linked audio/video edit coordination, deterministic ID generation order, fade fitting, and transactional undo/redo via `ChangeSet`.
- **Step 3C-1 (Implemented)**: Advanced trimming modes (`RollEdit`, `SlipClip`, `SlideClip`) with unified source-offset rounding toward zero, partner coordination, ambiguous role validation, and handle boundary limits.
- **Step 3C-2 (Implemented)**: Rate-stretch edit (`RateStretch`) and track push/pull (`ShiftTrackClips`) with fixed source range preservation, exact rational speed factor bounds (1/100x..100x), partner coordination, and scope-based ripple shifting. Step 3C complete.
- **Step 4 (Next)**: Keyframing and parameter animation.




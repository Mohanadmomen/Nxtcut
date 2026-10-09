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
- **Audio Fade Fitting**: Whenever an audio clip's duration shrinks or a clip is split (in `split_clip`, `trim_head`, `trim_tail`, ripple head trims, or overwrite strictly-contains splits), audio fades are fitted using `fit_audio_fades`: if `fade_in > clip.duration`, `fade_in = clip.duration`; if `fade_out > clip.duration`, `fade_out = clip.duration`.
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
## Roadmap: Steps 3B and 3C

- **Step 3B (Implemented)**: 12 timeline geometry editing commands (`AddClips`, `InsertClips`, `OverwriteClips`, `MoveClips`, `DeleteClips`, `SplitClip`, `TrimClip`, `RippleDeleteClips`, `CloseGap`, `JoinClips`, `LinkClips`, `UnlinkClips`) with frame snapping, ripple scoping, linked audio/video edit coordination, deterministic ID generation order, fade fitting, and transactional undo/redo via `ChangeSet`.
- **Step 3C (Next)**: Advanced trimming modes (roll edit, slip edit, slide edit, rate-stretch edit, timeline track push/pull).



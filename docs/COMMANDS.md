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
## Roadmap: Steps 3B and 3C

Step 3A establishes the core transactional mutation foundation. Subsequent steps will build upon this engine:
- **Step 3B**: Timeline geometry edits (insert edit, overwrite edit, timeline clip move, split clip, join clips, trim head/tail, ripple delete, close gap, linked audio/video edit coordination).
- **Step 3C**: Advanced trimming modes (roll edit, slip edit, slide edit, rate-stretch edit, timeline track push/pull).

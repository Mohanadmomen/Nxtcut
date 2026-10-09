# NxtCut Data Model (`model` Module)

## Overview

The `model` module (`nxtcut::model`) defines the pure document object model for NxtCut. It encapsulates multi-track timeline data, entity hierarchies, media references, visual transformation properties, and validation rules as plain C++20 data structures.

### Key Architectural Principles
- **Qt-Free and Headless**: No Qt types, GUI dependencies, or rendering code.
- **Pure Value Semantics**: All types are plain copyable data structures. A copy of a `Project` is an immutable snapshot that can be passed safely to background render pipelines.
- **Thread Safety**: Plain data, no internal synchronization; concurrent const access is safe; copies are independent.
- **Mutation Contract**: The model provides only data structures, queries, time mapping math, and validation. **Step 3 transactional commands are the only authorized mechanism for mutating the document state.**

---

## Entity Relationships

```
Project
 ├── main_sequence: SequenceId
 ├── media: map<MediaId, MediaAsset>
 │     └── VideoStreamInfo / AudioStreamInfo
 └── sequences: map<SequenceId, Sequence>
       ├── markers: vector<Marker>
       └── tracks: vector<Track>
             └── clips: vector<Clip>
                   ├── TransformProps
                   ├── effects: vector<EffectInstance>
                   └── content: ClipContent (variant)
                         ├── VideoContent (references MediaAsset)
                         ├── AudioContent (references MediaAsset)
                         ├── ImageContent (references MediaAsset)
                         ├── TextContent
                         └── CompoundContent (references Sequence)
```

### Entity Identifiers
Every major entity is referenced via strongly-typed handles derived from `core::Id<Tag>`:
- `ProjectId`
- `SequenceId`
- `TrackId`
- `ClipId`
- `MediaId`
- `EffectId`
- `MarkerId`
- `LinkId`

Generated via `generate_id<IdT>(core::UuidGenerator&)`, these types are strictly non-convertible across domain categories and are fully comparable for use as keys in `std::map`.

---

## Time Coordinate Systems

NxtCut models time using three strictly separated coordinate spaces wrapping `core::TimePoint` (based on $705,600,000$ ticks per second):

1. **`TimelineTime`**: Absolute position on a sequence timeline.
2. **`ClipTime`**: Offset from the beginning of a clip, expressed in timeline ticks.
3. **`SourceTime`**: Absolute position within the source media file or nested sequence.

### Playback Speed (`Speed`)
`Speed` models an exact rational playback multiplier $\frac{\text{numerator}}{\text{denominator}}$, stored in simplest fractional form (reduced by $\gcd$). For example, a playback speed of $2/1$ consumes $2 \times D$ source media ticks for every $D$ timeline ticks.

`Speed` provides exact equality comparisons (`operator==`), but deliberately does not provide relational ordering operators (`<`, `<=`, `>`, `>=`, `<=>`).

### Time Conversion Formulations

Scaling conversions (`clip_to_source`, `source_to_clip`, `source_span`) use `core::mul_div` (128-bit intermediate), and every conversion uses portable 64-bit overflow-checked addition and subtraction (`model::detail::checked_add`, `model::detail::checked_sub`) without platform- or compiler-specific builtins.

| Conversion | Signature | Valid Input Domain | Formula & Rounding |
| :--- | :--- | :--- | :--- |
| **Clip End** | `clip_end(clip)` | Any | $\text{start} + \text{duration}$ |
| **Timeline to Clip** | `timeline_to_clip(clip, t)` | $\text{start} \le t < \text{clip\_end}$ (half-open) | $t - \text{start}$ |
| **Clip to Timeline** | `clip_to_timeline(clip, c)` | $0 \le c \le \text{duration}$ (end-inclusive) | $\text{start} + c$ |
| **Clip to Source** | `clip_to_source(clip, c)` | $0 \le c \le \text{duration}$ (end-inclusive) | $\text{source\_in} + \lfloor c \cdot \frac{\text{num}}{\text{den}} \rfloor$ (`Floor`) |
| **Timeline to Source**| `timeline_to_source(clip, t)`| $\text{start} \le t < \text{clip\_end}$ | $\text{clip\_to\_source}(\text{timeline\_to\_clip}(t))$ |
| **Source to Clip** | `source_to_clip(clip, s)` | $s \ge \text{source\_in}$, result $\le \text{duration}$ | $\lfloor (s - \text{source\_in}) \cdot \frac{\text{den}}{\text{num}} \rfloor$ (`Floor`) |
| **Source Span** | `source_span(clip)` | Any | $\lceil \text{duration} \cdot \frac{\text{num}}{\text{den}} \rceil$ (`Ceil`) |

> [!NOTE]
> Round trips between `ClipTime` and `SourceTime` are exact only when rational division produces no fractional remainder.

---

## Clip Content Variants

Clips contain a `ClipContent` variant holding one of five specialized content descriptors:

1. **`VideoContent`**: References a visual media asset via `MediaId`. Sound is stored as a separate `AudioContent` clip on an audio track sharing the same `LinkId`.
2. **`AudioContent`**: References an audio or audiovisual media asset via `MediaId`. Holds animatable `volume` (linear gain, default 1.0) and fade-in/fade-out durations.
3. **`ImageContent`**: References an image media asset via `MediaId`. Must have `source_in == 0` and normal $1/1$ playback speed.
4. **`TextContent`**: Procedural text overlay specifying text string, font family, animatable `font_size_px` (default 48.0 px), and animatable `color` (default white). Must have `source_in == 0` and normal $1/1$ speed.
5. **`CompoundContent`**: References another sequence (`SequenceId`) within the same project. Enables nested timeline hierarchies.

---

## Compound Clip Cycle Rule

Nested compound clips form a directed graph where sequence $A \to B$ indicates that sequence $A$ contains a compound clip referencing sequence $B$.
- **Detection (`find_compound_cycles`, `find_compound_cycle`)**: Employs an iterative 3-color depth-first search (`White`, `Gray`, `Black`) with an explicit stack frame. Parallel edges between sequences are deduplicated preserving first-seen order. `find_compound_cycles` reports ONE cycle per back edge of the depth-first search (parallel edges count once); it does not enumerate every possible simple cycle; after one cycle is fixed, validating again may reveal another. `find_compound_cycle` returns the first detected cycle (or `std::nullopt` if acyclic). The graph algorithms use no recursion (a test nests 10,000 sequences), so deep nesting cannot overflow the stack inside them.
- **Cycle Prevention (`would_create_cycle`)**: Checks whether inserting a compound clip referencing child $C$ inside parent $P$ would introduce a cycle. Returns true if $C == P$ or if $P$ is reachable from $C$ through compound clips.

---

## Project Validation Rules

`collect_issues(const Project&)` performs complete, non-terminating document inspection and returns categorized `ValidationIssue` records in strictly deterministic order. `validate(const Project&)` returns `core::Status` (OK or `InvalidArgument` describing the first issue).

Validation issues are emitted in the following deterministic sequence:
1. **Project-level issues**: `MissingMainSequence`.
2. **Media map issues**: `IdMismatch` for media entries in map key order.
3. **Sequence-level issues** in map key order:
   - Sequence `IdMismatch` and `InvalidCanvas`.
   - Tracks in vector order: `DuplicateId` for track ID.
   - Clips in vector order: `DuplicateId` for clip ID, `InvalidDuration`, `NegativeStart`, `ClipRangeOverflow`, `ClipKindMismatch`, `DuplicateId` for effect IDs, and per-content media/source-range bounds checks (`MissingMedia`, `MediaKindMismatch`, `MissingSequence`, `SourceRangeOutOfBounds`).
   - Track `ClipOverlap` issues.
   - Markers in vector order: `DuplicateId` for marker ID.
4. **Compound cycle issues**: `CompoundCycle`, emitting one issue per cycle discovered by `find_compound_cycles`.
5. **Link issues**: in `LinkId` map order, checking `LinkIncomplete` and `LinkAcrossSequences`.

| Validation Code | Rule Enforced |
| :--- | :--- |
| `MissingMainSequence` | `project.main_sequence` must exist in `project.sequences`. |
| `IdMismatch` | Map key must equal `media.id` for `media` and `sequence.id` for `sequences`. |
| `DuplicateId` | `ClipId` and `TrackId` must be unique across the entire project; `MarkerId` unique within each sequence; `EffectId` unique within each clip. |
| `InvalidCanvas` | Sequence canvas width and height must be strictly positive ($> 0$). |
| `InvalidDuration` | Clip duration must be strictly positive ($> 0$). |
| `NegativeStart` | Clip start time must be non-negative ($\ge 0$). |
| `ClipRangeOverflow` | Clip range ($\text{start} + \text{duration}$) must not overflow 64-bit integer tick bounds. |
| `ClipOverlap` |    Clips on the same track must not overlap: a clip is reported when it starts before the latest end of all clips that precede it in start order, so overlap with a non-adjacent clip is also caught (touching half-open intervals $[t_1, t_2)$ and $[t_2, t_3)$ are permitted). |
| `ClipKindMismatch` | Video tracks admit Video, Image, Text, and Compound clips. Audio tracks admit only Audio clips. |
| `MissingMedia` | Referenced `MediaId` must exist in `project.media`. |
| `MediaKindMismatch` | Video clips require kind `Video` with video stream info; Image clips require kind `Image`; Audio clips require kind `Audio` or kind `Video` with audio stream info. |
| `SourceRangeOutOfBounds` | Video/Audio: $\text{source\_in} \ge 0$ and $\text{source\_in} + \text{source\_span} \le \text{media.duration}$. Image/Text: $\text{source\_in} == 0$ and $\text{speed} == 1/1$. Compound: $\text{source\_in} \ge 0$ and $\text{source\_in} + \text{source\_span} \le \text{sequence\_duration}$. |
| `LinkIncomplete` | Every `LinkId` must be shared by at least two clips. |
| `LinkAcrossSequences` | All clips sharing a `LinkId` must reside within the same sequence. |
| `MissingSequence` | Referenced compound sequence ID must exist in `project.sequences`. |
| `CompoundCycle` | Compound sequence nesting must be acyclic. Reported once per detected cycle. |
---

## Not Modelled or Not Enforced Yet

- Frame alignment: clip start and duration are not required to be multiples of the frame duration (audio clips often start between frames). Step 3 commands snap edits to frames.
- Reverse playback: `Speed` is always positive.
- Link timing: validation checks that a `LinkId` is shared by at least two clips in one sequence, not that the linked clips line up in time.
- Effect types: `EffectInstance::effect_type` is an opaque string; whether an effect exists is decided by the effects module and plugins.
- Shape, mask and Lottie clip kinds arrive in later steps.

---

## Exact Value Comparison (`identical`)

To detect no-op mutations and verify snapshot identity in tests without altering value semantics, `nxtcut::model::identical` provides exact field-by-field equality comparisons (`identical(const X&, const X&)`) across model structures:
- **Bit-Exact Floating-Point Comparisons**: All `double` values (such as `Property<double>`) and `float` color channels (inside `core::Color`) are compared bit-for-bit using `std::bit_cast` to same-size unsigned integers (`std::uint64_t` and `std::uint32_t`). Under this rule:
  - `0.0` and `-0.0` evaluate as **different** (differing sign bit).
  - Two identical quiet NaN values evaluate as **identical** (identical bit pattern).
- **Composite Types**:
  - `std::map` and `std::vector` compare sizes and elements in order.
  - Variants (`ClipContent`, `EffectParam`) compare the alternative index and active value.
  - Durations and timestamps compare integer ticks.
  - `Speed` compares via `operator==`.
- **No `operator==`**: Model types deliberately omit `operator==` to prevent accidental lossy or floating-point-compromised comparisons.
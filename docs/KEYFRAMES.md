# Keyframes Module (`engine/keyframes`)

## Purpose
The `keyframes` module provides pure temporal curve mathematics, interpolation types, and keyframe storage for parameter animation across the NxtCut editor. It depends solely on `nxtcut::core`.

Speed is a primary design constraint: property evaluation occurs per animated property, per clip, per frame during playback and scrubbing. The evaluation path is completely allocation-free, `noexcept`, and offers amortized $O(1)$ complexity for sequential playback.

---

## Core Types

1. **`EasingKind` (`easing.hpp`)**:
   Scoped enumeration defining 19 standard easing functions: `Linear`, and `EaseIn<F>`, `EaseOut<F>`, `EaseInOut<F>` across six families (`Sine`, `Quad`, `Cubic`, `Quart`, `Expo`, `Circ`).
   Evaluated with `ease(EasingKind, double t)`. Inputs are clamped to $[0, 1]$; NaN evaluates as $0.0$; endpoints evaluate to $0.0$ and $1.0$ exactly. Canonical string conversion is provided via `to_string` and `easing_from_string`.

2. **`Interpolation` (`interpolation.hpp`)**:
   Small, trivially-copyable value type defining segment progress mapping:
   - `Hold`: Step function ($0.0$ for $t < 1.0$, $1.0$ for $t \ge 1.0$).
   - `Linear`: Identity ($t$).
   - `Easing`: Evaluates one of the 19 preset easing functions.
   - `Bezier`: Evaluates CSS-style cubic-bezier curves ($P_0=(0,0), P_1=(x_1,y_1), P_2=(x_2,y_2), P_3=(1,1)$). $x_1, x_2 \in [0, 1]$; $y_1, y_2$ allow overshoot. Precomputes polynomial coefficients; solves $x(s) = t$ using Newton-Raphson with bisection fallback.
   `Hold` and `Linear` progress mappings are evaluated inline in header files.

3. **`Animatable<T>` and `AnimatableTraits<T>` (`animatable.hpp`)**:
   Trait-based extension point defining interpolation, finiteness checks, exact bit-level equality, and default fallback values for animatable types. Specializations provided:
   - `double`
   - `core::Color` (delegates to `core::lerp`; channel range is unclamped for overshoot support)
   - `core::Point<double>`

4. **`Keyframe<T>` (`keyframe.hpp`)**:
   Plain struct containing `core::TimePoint time`, `T value`, and `Interpolation interpolation`. The interpolation describes the segment from this keyframe to the next and is ignored on the final keyframe.

5. **`KeyframeTrack<T>` (`keyframe_track.hpp`)**:
   Contiguous time-ordered keyframe sequence providing fast evaluation.

---

## Invariants

- **Monotonicity**: Keyframes are strictly ordered by ascending time (`keys[i].time < keys[i+1].time`). Duplicate timestamps are forbidden.
- **Finite Values**: Every keyframe value must satisfy `AnimatableTraits<T>::is_finite(val)`.
- **Bounded Time**: All keyframe timestamps must reside within $[-kMaxKeyframeTicks, +kMaxKeyframeTicks]$ where $kMaxKeyframeTicks = 2^{62}$. This guarantees that tick differences cannot overflow signed 64-bit integers.

---

## Complexity Table

| Operation | Method | Complexity | Notes |
| :--- | :--- | :--- | :--- |
| Construction | `KeyframeTrack()` | $O(1)$ | Empty track |
| Validation / Creation | `KeyframeTrack::create(keys)` | $O(n)$ | Validates sort order, bounds, and finiteness |
| Size / Empty / Keys | `size()`, `empty()`, `keys()` | $O(1)$ | No-copy span access |
| Timestamp Search | `find(t)` | $O(\log n)$ | Binary search (`std::lower_bound`) |
| Insertion / Update | `set(key)` | $O(n)$ | Keeps order; replaces duplicate timestamps |
| Removal | `remove_at(t)` | $O(n)$ | Vector element removal |
| Evaluation (Random) | `value_at(t)` | $O(\log n)$ | Binary search (`std::upper_bound`) |
| Evaluation (Sequential) | `value_at(t, Cursor&)` | $O(1)$ amortized | Checks current segment, then next segment, then binary search |

---

## Performance Rules

1. **Zero Heap Allocation**: Evaluation never allocates heap memory.
2. **`noexcept` Guarantees**: `value_at`, `map_progress`, and `ease` are strictly `noexcept`.
3. **Inlined Fast Paths**: Linear and Hold interpolation branches are inlined in headers.
4. **Horner Polynomials**: Cubic-bezier coefficients are computed once upon factory construction; evaluation uses Horner polynomial multiplication.
5. **Cursor Caching**: Sequential frame-by-frame timeline playback tests the current and consecutive segments before falling back to binary search, achieving $O(1)$ amortized execution.
6. **Bit-Identical Evaluation**: `value_at(t)` and `value_at(t, cursor)` return bit-identical results for identical timestamps.

---

## Integration with `model::Property`

In Step 4B, `nxtcut::model::Property<T>` integrates keyframe animation for animatable types (`double` and `core::Color`):
- **Shared Immutable Tracks**: `Property<T>` stores an optional `std::shared_ptr<const KeyframeTrack<T>>` alongside the constant value. Tracks are immutable; updates perform copy-on-write (`std::make_shared`), allowing document snapshots and history copies to remain cheap refcount increments.
- **Evaluation & Caching**: `Property::value_at(ClipTime)` evaluates in $O(1)$ for constant properties and $O(\log n)$ for animated properties. Sequential playback utilizes `Property::value_at(ClipTime, KeyframeTrack<T>::Cursor&)` for amortized $O(1)$ evaluation, producing bit-identical results.
- **Equality Comparison**: `model::identical` verifies bit-exact equality of constant values and keyframe tracks (`keyframes::identical`).

---

## Deferred Scope

The following capabilities are intentionally deferred to future steps:
- **Step 4C**: Transactional editing commands (`AddKeyframe`, `MoveKeyframe`, `RemoveKeyframe`, `SetKeyframeInterpolation`, trim shift/ripple).
- **Post-4C**: Spring physics simulation curves and motion modifiers (wiggle, overshoot, inertia).

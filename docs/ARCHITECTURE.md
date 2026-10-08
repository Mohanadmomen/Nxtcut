# NxtCut-CPP Architecture Guide

## Overview

NxtCut-CPP is structured as a strictly layered, multi-track desktop video editor inspired by modern timeline editors.
The codebase is cleanly separated into two distinct architectural tiers:

1. **`engine/`**: A pure, UI-free C++20 library containing all core video editing logic, data models, rendering pipelines, audio mixing, decoding, encoding, and plugin interfaces. **The engine must NEVER include or link Qt.**
2. **`app/`**: A Qt 6 desktop application built with Qt Widgets that consumes the engine. The application depends on the engine; the engine never depends on or references the application.

This separation guarantees that the engine remains fully headless, testable in isolation, scriptable for CLI or server rendering pipelines, and free from UI framework coupling.

---

## Planned Engine Modules

Engine modules are introduced incrementally in a strict dependency sequence:
`core` &rarr; `model` &rarr; `commands` &rarr; `keyframes` &rarr; `storage` &rarr; `media` &rarr; `playback` &rarr; `render` &rarr; `effects` &rarr; `audio` &rarr; `export` &rarr; `plugins` &rarr; `analysis`.

### 1. `core`
Provides fundamental utility primitives, domain-specific math types, error handling abstractions (`Result<T>`), time models, conversions, and engine version metadata used universally across all downstream modules.

Core module components:
- **Error Modeling (`error.hpp`, `result.hpp`)**: `ErrorCode` enumeration and `Error` class providing context chaining (`with_context()`). Value-based `Result<T>` and `Status` (`tl::expected`) error returns with `make_error()` helper for zero-exception operational failure handling.
- **Checked Arithmetic (`mul_div.hpp`)**: Portable `mul_div()` computing `round(a * b / c)` with a 128-bit intermediate (via `__int128`, MSVC x64 intrinsics `_umul128`/`_udiv128`, and a software fallback) and configurable rounding (`Floor`, `Nearest`, `Ceil`).
- **Time Model (`time.hpp`)**: Strong types `TimePoint` and `Duration` based on an integer tick clock, and `TimeRange` half-open interval `[start, end)` math.
- **Frame Rate & Sample Rate (`frame_rate.hpp`)**: Rational `FrameRate` with GCD reduction and exact fraction comparison, plus `SampleRate` for audio.
- **Conversions (`frame_time.hpp`)**: Discrete `FrameIndex` conversions (`frame_to_time`, `time_to_frame`, `snap_to_frame`, `samples_to_time`, `time_to_samples`) with zero accumulated drift.
- **Timecode (`timecode.hpp`)**: SMPTE timecode supporting non-drop and drop-frame accounting (for 29.97 and 59.94 fps), parsing, formatting, and frame index round-tripping.
- **Identifiers (`uuid.hpp`, `id.hpp`)**: RFC 4122 version-4 `Uuid`, thread-safe `UuidGenerator`, and type-safe `Id<Tag>` strong entity handles.
- **Logging Subsystem (`log_level.hpp`, `log_sink.hpp`, `log_format.hpp`, `logger.hpp`, `console_sink.hpp`, `file_sink.hpp`, `memory_sink.hpp`)**: Hierarchical, non-global, dependency-injected logging. Callers construct and pass `Logger` instances configured with `LogLevel` and shared `LogSink` implementations (`ConsoleSink`, `FileSink`, `MemorySink`). Deterministic civil calendar line formatting (`format_log_line()`) using integer floor arithmetic.
- **Cancellation Model (`cancellation.hpp`)**: Cooperative, one-shot cancellation primitives. `CancellationSource` owns exclusive cancellation lifecycle with release-store semantics, issuing cheap copyable `CancellationToken` handles observing state with acquire-load semantics.
- **Move-Only Tasks (`unique_task.hpp`)**: Type-erased, move-only zero-argument callable wrapper enabling `std::packaged_task` and lambdas capturing `std::unique_ptr` without requiring copy construction.
- **Thread Pool (`thread_pool.hpp`)**: Work-dispatching thread pool with prioritized FIFO scheduling (`High`, `Normal`, `Low`). Worker threads execute tasks from higher-priority queues first, catching exceptions through `std::packaged_task` futures without worker death, supporting cooperative `shutdown()` and `wait_idle()`.
- **Spatial Geometry Primitives (`geometry.hpp`)**: Strongly constrained `Point<T>`, `Size<T>`, and `Rect<T>` templates over arithmetic types. Half-open `[left, right)` and `[top, bottom)` intervals, with non-overlapping touching edge semantics, bounding box unions, and aspect-ratio helpers (`fit_inside`, `fill_outside`, `letterbox_rect`).
- **Affine Transformations (`affine_transform.hpp`)**: Column-vector 2D affine transform matrix with explicit composition chaining (`t.then(n)` applies `t` first, then `n`), point and bounding-box rect application, determinant, and numerical inversion.
- **Color Pipeline (`color.hpp`)**: Straight-alpha `Color` and separate `PremultipliedColor` types to prevent unintended alpha blending bugs. Full hex string parsing and formatting, component-wise interpolation (`lerp`), and exact IEC 61966-2-1 sRGB $\leftrightarrow$ Linear transfer function conversions.

#### Core Subsystem Design Models

##### Logging Model
The logging subsystem strictly prohibits global mutable state and singletons. Loggers are explicit objects instantiated by callers and injected into dependent components.
- **Sinks (`LogSink`)**: Thread-safe abstract destinations receiving `LogRecord`. Multiple loggers share sinks safely via `std::shared_ptr`.
- **Logger (`Logger`)**: Atomic minimum level filtering, child hierarchy naming (`<parent>.<child>`), immutable sink lists, and custom injected timestamps for deterministic testing.
- **Sinks Provided**: `ConsoleSink` (unbuffered stream writing under mutex), `FileSink` (binary-mode appending/truncating with automatic flush on `Warn` or above), and `MemorySink` (bounded ring buffer feeding in-memory diagnostic snapshot inspection).

##### Cancellation Model
- Cooperative cancellation uses `CancellationSource` and `CancellationToken`.
- `CancellationSource` represents an unmovable, uncopyable cancellation lifecycle domain. Calling `request_cancel()` is idempotent and publishes cancellation with release memory ordering.
- `CancellationToken` is cheap and copyable, loading state with acquire memory ordering. A default-constructed token is valid and never cancelled. Tokens outlive sources safely via shared atomic state.

##### Thread Pool & Prioritization
- Fixed worker pool created via `ThreadPool::create(thread_count)`.
- Implements three FIFO queues: `High`, `Normal`, and `Low`. Workers greedily drain `High` priority tasks before `Normal`, and `Normal` before `Low`. Tasks within the same priority level execute in strict FIFO order.
- Tasks are submitted via `submit(f, priority)` which captures callables into `std::packaged_task` within `UniqueTask`. Exceptions are preserved in the returned `std::future`.
- `shutdown()` is idempotent, finishes all currently queued tasks, rejects new submissions, and joins workers. `wait_idle()` blocks until all queues and worker threads are completely idle.

##### Spatial Geometry & Affine Transforms
- Geometry templates (`Point`, `Size`, `Rect`) enforce half-open interval semantics `[x, x + width)` and `[y, y + height)`. Touching boundaries do not intersect, and empty rectangles never contain points or overlap other rectangles.
- `AffineTransform` uses column vectors ($\mathbf{x}' = \mathbf{M} \mathbf{x}$).
- Composition order is explicitly defined by `t.then(n)`: transform `t` is applied first, followed by transform `n` (`t.then(n).apply(p) == n.apply(t.apply(p))`).
- Positive rotation angles rotate the positive x-axis towards the positive y-axis (clockwise in top-left screen coordinates).

##### Color Model
- `Color` represents straight (un-premultiplied) RGBA color values with float channels nominally in $[0, 1]$.
- `PremultipliedColor` represents $(r \cdot a, g \cdot a, b \cdot a, a)$ as a separate, non-implicitly-convertible type to guarantee that alpha blending occurs in premultiplied space.
- Transfer functions `srgb_to_linear()` and `linear_to_srgb()` adhere to the standard piecewise IEC 61966-2-1 transfer curve with clamping.

#### The NxtCut Time Model

Professional multi-track timeline editing requires sub-frame precision, audio-sample alignment, and zero drift across long projects. The NxtCut time architecture adheres to three core design principles:

1. **Integer Ticks instead of Floating-Point**:
   Floating-point representations (`double` or `float`) suffer from non-associative rounding and representation error that compounds when accumulating durations or converting between frame indices and timeline timestamps. Over hours of multi-track media playback, floating-point inaccuracies cause audio/video drift and misaligned edit boundaries. Using 64-bit signed integer ticks guarantees exact arithmetic, associativity, deterministic serialization, and spans approximately $\pm 414$ years without overflow.

2. **Why 705,600,000 Ticks per Second (`kTicksPerSecond`)**:
   The master timeline clock frequency is defined as $705,600,000 = 2^9 \times 3^2 \times 5^5 \times 7^2$ ticks per second. This frequency is the lowest common multiple that evenly divides:
   - All standard film and video frame rates: 24, 25, 30, 48, 50, 60, 100, 120 fps.
   - Fractional NTSC broadcast frame rates: $24000/1001$ (23.976 fps), $30000/1001$ (29.97 fps), and $60000/1001$ (59.94 fps). For instance, at 29.97 fps, one frame is exactly $\frac{705,600,000 \times 1001}{30000} = 23,543,520$ integer ticks.
   - All professional audio sampling rates: 44.1 kHz ($16,000$ ticks/sample), 48 kHz ($14,700$ ticks/sample), 96 kHz ($7,350$ ticks/sample), and 192 kHz ($3,675$ ticks/sample).
   Because every standard video frame and audio sample period corresponds to an exact integer number of ticks, time math produces no rational remainders under standard rates.

3. **How Conversions Avoid Drift**:
   Downstream systems must never track playback or clip placement by repeatedly adding per-frame durations $\Delta t$. Doing so accumulates rounding adjustments when rates have fractional periods. Instead, NxtCut conversions always evaluate directly from the absolute frame index:
   $$\text{time} = \text{round}\left(\frac{\text{frame} \times \text{denominator} \times kTicksPerSecond}{\text{numerator}}\right)$$
   using `mul_div()` with 128-bit intermediate precision and `Nearest` rounding (half away from zero). Consequently, frame $N$ always maps to the exact same timeline instant regardless of scrubbing history, timeline zoom, or playback speed.
### 2. `model`
Defines the core document object model for multi-track video editing, including project metadata, timelines, audio/video tracks, clip entities, transitions, and timeline markers without any execution logic or side effects.

### 3. `commands`
Implements the command pattern for all user timeline operations, providing transactional edit commands, undo/redo history stacks, batch command grouping, and document state mutation.

### 4. `keyframes`
Manages parameter animation over time, implementing keyframe storage, interpolation algorithms (linear, bezier, easing curves), parameter evaluation at arbitrary timestamps, and spatial/temporal curve math.

### 5. `storage`
Handles project persistence, serialization, and deserialization of the timeline model and project assets to disk formats (such as JSON or binary archives), including asset relocation, file hashing, and project bundling.

### 6. `media`
Encapsulates low-level multimedia container demuxing, video stream decoding, and audio stream decoding using FFmpeg, providing frame caching, hardware-accelerated decode handles, and stream metadata probing.

### 7. `playback`
Manages real-time timeline playback scheduling, master clock synchronization between audio and video streams, playhead scrubbing, reverse playback, and pre-roll buffering.

### 8. `render`
Implements the visual composition and frame rendering pipeline, supporting multi-track blending, alpha compositing, track transformations, and frame rasterization across swappable CPU and GPU render backend interfaces.

### 9. `effects`
Provides visual and audio filter graphs, color grading, adjustments, and procedural effects applied to individual clips or adjustment tracks during the rendering pipeline.

### 10. `audio`
Manages multi-channel audio mixing, resampler processing, track volume envelopes, pan balancing, real-time playback streaming via audio device backends, and master bus limiting.

### 11. `export`
Coordinates the offline timeline rendering and encoding pipeline, feeding composite video and mixed audio into FFmpeg encoders to produce final rendered container files with progress tracking and cancellation.

### 12. `plugins`
Exposes clean C-ABI and C++ plugin boundaries allowing external developers to create third-party effects, transitions, media decoders, and procedural generators without access to engine internals.

### 13. `analysis`
Performs non-blocking media analysis operations on background threads, including audio waveform generation, optical flow motion analysis, scene cut detection, and silence detection.

---

## Engine Dependency Table

The engine enforces an acyclic dependency graph. The table below defines the exact allowed dependencies for each engine module:

| Module | Allowed Dependencies | Purpose Summary |
| :--- | :--- | :--- |
| `core` | *(None)* | Primitives, types, error models, versioning |
| `model` | `core` | Timeline entities, tracks, clips |
| `keyframes` | `core` | Temporal curves and interpolation math |
| `commands` | `core`, `model`, `keyframes` | Undo/redo and transactional mutations |
| `storage` | `core`, `model`, `keyframes` | Project file serialization and loading |
| `media` | `core` | FFmpeg demuxing and decoding wrappers |
| `playback` | `core`, `model`, `media` | Real-time playhead scheduling and audio/video clock sync |
| `render` | `core`, `model`, `keyframes`, `media` | Multi-track frame composition and GPU/CPU backends |
| `effects` | `core`, `keyframes`, `render` | Image filters, shaders, and visual transforms |
| `audio` | `core`, `model`, `media` | Multi-track mixing, resampling, and audio output |
| `export` | `core`, `model`, `media`, `render`, `audio`, `playback` | Offline encoding and disk export pipeline |
| `plugins` | `core`, `effects`, `render` | External plugin ABI and host interfaces |
| `analysis` | `core`, `media` | Background waveform and scene analysis |

This dependency contract is verified automatically by `scripts/check_architecture.py` during both local builds and CI runs.

---

## Architectural Rule: Engine UI-Freedom

**Rule**: No file in `engine/` may ever include a Qt header (`<Q...>` or `<Qt...>`) or link a Qt library (`Qt6::*`).

### Rationale
- **Decoupling**: Decoupling the engine from the GUI framework allows the entire core editor logic to be tested in headless environments without X11, Wayland, or Windows display contexts.
- **Portability**: Enables embedding the engine in CLI tools, server batch rendering jobs, or alternative front-ends (e.g. mobile, web, or embedded) without dragging in Qt.
- **Compilation Speed**: Prevents Qt meta-object compilation (moc) from affecting the core library.

Violations are caught by `scripts/check_architecture.py` and cause immediate test failure.

---

## How to Add a New Engine Module

When implementing the next engine module in sequence, follow these steps:

1. **Create directory structure**:
   ```
   engine/<module_name>/
     CMakeLists.txt
     include/nxtcut/<module_name>/
     src/
   ```

2. **Define `CMakeLists.txt` using `nxtcut_add_module`**:
   ```cmake
   nxtcut_add_module(
       NAME <module_name>
       DEPENDS
           # Public dependencies exposed in headers
           nxtcut::<allowed_dep>
       PRIVATE_DEPENDS
           # Implementation dependencies
           fmt::fmt
   )
   ```

3. **Register the module in `engine/CMakeLists.txt`**:
   ```cmake
   add_subdirectory(<module_name>)
   ```

4. **Verify architectural rules**:
   Ensure all `#include <nxtcut/...>` statements respect the `ALLOWED_DEPENDENCIES` table in `scripts/check_architecture.py`.

5. **Create unit tests**:
   ```
   tests/<module_name>/
     CMakeLists.txt
     <feature>_test.cpp
   ```
   Configure `tests/<module_name>/CMakeLists.txt`:
   ```cmake
   nxtcut_add_module_test(
       NAME <module_name>
       SOURCES
           <feature>_test.cpp
   )
   ```
   And register in `tests/CMakeLists.txt`:
   ```cmake
   add_subdirectory(<module_name>)
   ```

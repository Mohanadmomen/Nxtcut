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
Provides fundamental utility primitives, math types, error handling abstractions (`Result<T>`), logging interfaces, profiling hooks, and engine version metadata used universally across all downstream modules.

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

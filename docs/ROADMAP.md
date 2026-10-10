# NxtCut - Roadmap

The full plan. `docs/PROJECT_STATE.md` says where we are right now; this file says where we are going.
Each step ends with code that builds, passes its tests on the maintainer's machine and in CI, and is merged
through a branch and a green pull request.

Priority order for every decision: (1) clean, modular, tested code; (2) correctness; (3) performance; (4) features.

## Engine steps (all Qt-free, in `engine/`)

| # | Step | Scope | Depends on | State |
|---|---|---|---|---|
| 0 | Foundation | CMake, vcpkg, GoogleTest, warnings, sanitizers, architecture script, CI, docs | - | done |
| 1 | Core types | errors, mul_div, time, frame rate, conversions, timecode, uuid, ids | 0 | done |
| 1B | Core utilities | logger and sinks, cancellation, thread pool, geometry, affine transform, color | 1 | done |
| 1C | Cleanup | portable mul_div path with tests, `.gitattributes` | 1B | done |
| 2 | Data model | Project, Sequence, Track, Clip, media references, properties, IDs, validation, compound-clip graph | 1 | done |
| 3A | Commands infrastructure | ChangeSet, apply/inverse, History, Editor, transactions, listeners, 15 simple commands, `model::identical` | 2 | done |
| 3B | Timeline edits | add, insert, overwrite, move, delete, split, trim, ripple delete, close gap, join, link, unlink | 3A | done |
| 3C-1 | Advanced edits (1) | roll, slip, slide; unify source-offset rounding between ripple and non-ripple trims | 3B | NEXT |
| 3C-2 | Advanced edits (2) | rate-stretch (speed limited to 1/100x..100x), track push/pull (`ShiftTrackClips`) | 3C-1 | todo |
| 4 | Keyframes | curves, bezier, easing presets, spring, interpolation, animated properties, motion modifiers, keyframe commands. Must first settle the `model -> keyframes` dependency | 1B, 2, 3A | todo |
| 5 | Storage | JSON project files, schema version and migrations, workspace layout, autosave, trash, ZIP bundles, orphan cleanup | 2, 4 | todo |
| 6 | Media layer | FFmpeg probe, decode, frame-accurate seek, hardware-decode hooks, thumbnails, waveforms, proxies, relink, ProRes. Consider a tiny FFmpeg spike on all CI platforms around Step 5 | 1B | todo |
| 7 | Playback | master clock, decode-ahead scheduler, frame cache, scrubbing, adaptive quality | 3, 6 | todo |
| 8 | Render core (CPU) | `RenderBackend`, frame buffers, compositor, transform/crop/corner-pin, 25 blend modes, masks, opacity | 4, 7 | todo |
| 8b | Text and vector | font layout, shapes, SVG, Lottie, animated text | 8 | todo |
| 9 | Effects and transitions | effect and transition interfaces; blur, color (levels, curves, wheels, .cube LUT, hue/sat), distortion, stylize, chroma key; fades, wipes, slides, iris, dissolve | 8 | todo |
| 10 | Audio engine | mixer, volume, fades, 6-band EQ, pitch, meters, resampling, A/V sync, `AudioOutput` | 7 | todo |
| 11 | Export | MP4, WebM, MOV, MKV; H.264, H.265, VP9, AV1 where available; AAC, MP3, WAV; subtitles; presets; progress; cancel | 8, 10 | todo |
| 12 | GPU backend | second `RenderBackend`, shaders, scopes, parity tests against the CPU reference | 8, 9 | todo |
| 13 | Plugins | native plugin C API, OpenFX host, VST3/CLAP hosting | 9, 10 | todo |
| 14 | Analysis and AI | scene detection, transcription, captions, semantic search | 6 | todo |
| 15 | Optional generation | local TTS, music generation | 14 | todo |

Also needed for M1: a small `nxtcut-cli` tool (in `tools/`) that loads a project and exports a video.
Step sizes are very uneven: media, render and export are much larger than the steps done so far.

## App steps (Qt Widgets UI, after M1)

| # | Step | Scope |
|---|---|---|
| A0 | Skeleton | main window, dark theme, HiDPI, settings, log panel (a `LogSink`) |
| A1 | Engine adapter | QObject wrappers, signals, thread marshalling, `EditListener` adapter |
| A2 | Workspace and projects | open, create, recent projects |
| A3 | Media library | import, thumbnails, metadata |
| A4 | Viewer | OpenGL preview, gizmos |
| A5 | Timeline | custom-painted tracks, clips, filmstrips, waveforms, snapping, zoom, markers, edit tools, sequence tabs |
| A6 | Inspector | clip and effect properties |
| A7 | Keyframe editor | dopesheet and graph |
| A8 | Effects and transitions browser | |
| A9 | Audio mixer | |
| A10 | Source monitor | |
| A11 | Export dialog | |
| A12 | Settings and hotkeys | |
| A13 | Scopes | |
| A14 | Scene browser and AI panels | |

## Milestones

| M | Meaning |
|---|---|
| M1 | Headless engine with command-line export (steps up to 11 plus `nxtcut-cli`) |
| M2 | Qt UI alpha: edit, preview and export with the UI (A0 to A11) |
| M3 | Effects, transitions, keyframes and masks usable in the UI |
| M4 | Audio polished, A/V sync verified |
| M5 | GPU backend on all platforms with parity tests |
| M6 | Plugins (OpenFX, VST3, CLAP) with a sample plugin |
| M7 | Local AI features |
| M8 | v1.0: installers, signed builds, docs, notices |

## FreeCut feature to module map

| FreeCut feature | NxtCut module |
|---|---|
| Multi-track timeline, sequences, compound clips | model, commands |
| Split, ripple, rolling, slip, slide, rate-stretch, linked A/V | commands |
| Transitions | effects, commands |
| Keyframes, easing, motion modifiers, motion text | keyframes, text |
| Preview, clock, scrubbing, adaptive quality | playback, render |
| Effects, masks, blend modes, chroma key, LUTs | render, effects |
| GPU scopes | render (GPU), app |
| Audio fades, EQ, pitch, meters, master bus | audio |
| Import (video, audio, images, GIF, SVG, Lottie, ProRes), proxies, relink | media, vector |
| Workspace folder, ZIP bundles, autosave, trash | storage |
| Export (containers, codecs, subtitles) | export |
| Transcription, captioning, scene detection, TTS, music | analysis, AI modules |
| Hotkeys, templates, settings | app, storage |

When porting behaviour from FreeCut, port the behaviour, not the TypeScript structure. Both projects are MIT;
keep notices in `THIRD_PARTY_NOTICES`.

## Open decisions (recommended default in brackets)

| Topic | Recommendation |
|---|---|
| GPU backend placement | engine cannot use Qt RHI; raw OpenGL behind a `GraphicsContext` interface supplied by the app; revisit Vulkan/Metal at Step 12 |
| Internal pixel format | RGBA16F on GPU, RGBAF32 or RGBA8 on CPU, premultiplied; decide in Step 8 |
| Color management | sRGB / Rec.709 only, leave a hook |
| Audio output | miniaudio behind an `AudioOutput` interface (verify license) |
| Pitch shifting | SoundTouch (LGPL; check linking rules) or a phase vocoder later |
| Text and fonts | FreeType + HarfBuzz (choose the FreeType FTL license) |
| SVG / Lottie / shapes | ThorVG (MIT; verify) |
| Docking UI | Qt Advanced Docking System (LGPL; verify) or custom; avoid KDDockWidgets (GPL) |
| Plugins | OpenFX for video; VST3 and CLAP for audio; Adobe plugins cannot be loaded |
| Local AI | whisper.cpp, ONNX Runtime (check model-weight licenses); late, optional |
| Hardware decode | via FFmpeg hwaccel (D3D11VA/NVDEC, VideoToolbox, VAAPI) with software fallback |
| FFmpeg spike | tiny FFmpeg-from-vcpkg test on all CI platforms around Step 5 (proposed, not yet confirmed) |

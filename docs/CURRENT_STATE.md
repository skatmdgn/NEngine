# NEngine current state

Status snapshot: **2026-10-17 (KST)**, reviewed against `main` commit [`51db15f`](https://github.com/skatmdgn/NEngine/commit/51db15f79eaa443368e956cada956884aa7dec48).
Development version: **0.9.0-dev** (pre-1.0; the number identifies the active standalone-Player integration milestone, **not** completed 0.9 scope).
Authoritative product target: [MASTER_PLAN.md](MASTER_PLAN.md). Detailed feature checkboxes: [ROADMAP.md](ROADMAP.md). Detailed historical implementation notes: [history/CURRENT_STATE_THROUGH_2026-10-16.md](history/CURRENT_STATE_THROUGH_2026-10-16.md).

## What is verified and what is not

- **Repository inspection:** CMake targets/source tree and representative public headers were checked; this is **not** an exhaustive line-by-line code audit.
- **Latest known CI:** [NEngine CI #1037](https://github.com/skatmdgn/NEngine/actions/runs/38903656615), commit `51db15f`, completed 2026-10-16 UTC: **11/11 CTest groups passed on Ubuntu and 11/11 passed on Windows**. Tests cover Core, Editor, Assets, Scripting, Render, Input, Physics, Audio, UI, Player, and Player CLI. Both builds and Windows executable artifact uploads succeeded.
- **Historical user acceptance:** Windows editor 0.2.6 previously opened successfully; Hierarchy, Inspector, rename/Transform, Undo/Redo, Play/Pause/Step/Stop, .nscene save/load, and Korean names were checked by the user.
- **Not verified by CI:** Production Windows Vulkan GPU/rendering, audible playback on physical devices, a complete interactive packaged .NET game, cross-vendor drivers, long-running stability, and Android device/build QA. These must not be presented as passed.

## Architecture implemented in the repository

```text
apps/editor   -> engine/editor (EditorModel)
apps/player   -> engine/player (PlayerRuntime / bundle / exporter)
engine/editor -> engine/core, assets, render, physics, audio, input, scripting, ui
engine/player -> engine/core, assets, render, physics, audio, input, scripting
engine/render -> engine/core, assets
engine/physics-> engine/core
engine/scripting -> engine/core, input
engine/ui     -> engine/core, input, assets
engine/core   -> World / component pools / Scene / Prefab / serialization
engine/platform -> native Win32 window and portable stub
```

This is a **dependency overview**, not a complete generated dependency graph. The target 20-subsystem architecture includes planned packages, navigation, jobs, Android toolchains and profiling that are **not** all implemented as separate modules yet.

## Implemented foundations (code + focused automated tests)

| Area | Present in source | Important boundary |
| --- | --- | --- |
| Core / World / Scenes | Generational entities, hierarchy, component registry/pools, Scene v3 codecs, references, cloning and Prefab foundation | Full production Prefab workflow/variants remain incomplete |
| Windows Editor | Native Win32 shell, panes, Inspector editing, history, scene/project workflows, diagnostic Scene View, optional Vulkan Preview | Default edit viewport is still GDI; complete Vulkan Game View and freeform docking not finished |
| Assets | GUID/.meta DB, importer/cache/dependency watcher, glTF/GLB/OBJ/MTL, texture/material/shader foundations | FBX, WebP decode/transcoding and complete external resource policy remain |
| Rendering / 2D | Vulkan Win32 swapchain, buffers, depth, material/mesh/texture paths, SpriteRenderer, sprite clips/controllers | Production PBR/lighting/shadows, modern Vulkan editor/game view, Android surface require work |
| Scripting | C# hostfxr, generated projects/assemblies, lifecycle and hot reload; native managed property/query/UI bridges | Packaged API/debugger support, comprehensive bindings and third-party managed dependency resolution not finished |
| Physics 2D/3D | Body/collider contacts, impulse/mass/inertia solver, queries, sleeping, managed callbacks | Additional shapes/casts, production tuning and hardware integration QA remain |
| Audio and Input | WAV decode/WASAPI, native playback, keyboard/mouse, simple action maps | Compressed streaming, real hardware acceptance, gamepad/touch and full action persistence remain |
| Runtime UI / Animation | Native Canvas model, authoring draft/Undo, GDI diagnostic overlays, C# UI bridge, sprite state graphs | Vulkan runtime UI, production text rendering, general skeletal/blend-tree animation, navigation/particles incomplete |
| Standalone Windows Player (0.9 partial) | Bundle/CLI exporter, World simulation, WAV/sprite, prebuilt managed-game DLL hosting, 2D/3D events, texture + model/.nmat descriptor cooking, focused Win32 keyboard/mouse and 2D/3D queries | Not yet a generally shippable Windows game exporter; imported material subasset cooking, wider API parity, production GPU/.NET/input QA missing |
| Android / SDK plugins | Windows+Android target and extension policy documented | APK/AAB, Vulkan Android surface, toolchain profiles, native/plugin integration still unimplemented |

- **Rigidbody parity update:** Standalone Player managed native property bridge supports both 3D and 2D Rigidbody, including Use Gravity, Is Kinematic, Allow Sleep, Sleeping, Mass, Gravity Scale, Sleep Threshold, Linear Velocity and Angular Velocity. Invalid types/nonfinite values are rejected, and velocity edits wake sleeping bodies. These are focused regression-tested code paths; interactive .NET Player hardware acceptance is still pending.

- **Render property bridge:** Player C# native property calls can now read/write Camera, Light, SpriteRenderer and MeshRenderer attributes and AssetGuid references, with finite/range/type checks and portable tests. This does not imply production Vulkan Game View or real driver acceptance.

- **Audio property bridge:** The standalone Player now exposes C# AudioSource clip, playback, time, pitch, pan, spatialization/distance and AudioListener volume/enabled properties with validation, including clip-change playback reset tests. Streaming decoding and real WASAPI device QA remain pending.

## Highest-priority next work

1. **Player completion:** exported glTF/OBJ generated material/texture subassets, managed action map/property parity, and genuine Windows GPU + .NET + input acceptance.
2. **Shippable runtime:** Vulkan Game View/editor picking, runtime Canvas textures/text, renderer PBR/shadow validation, and packaging/dependency deployment.
3. **Cross-platform:** Android Vulkan surface, versioned SDK/NDK/JDK/Gradle profiles, APK/AAB exporter and touch input.
4. **Ecosystem:** managed/native plugin contracts and NuGet/runtime resolution, Visual Studio debugger attach/symbol workflow.
5. **Subsystem depth:** stable/extended physics queries, production audio streaming and real-time scheduling, broader animation/navigation/particle features, asset import extensions.

Priorities are **not** claims that later areas are already done; see unchecked items in ROADMAP. Review and adjust this snapshot after every significant functional milestone; do not append dated progress paragraphs above the heading. Keep historical logs in `docs/history/`.

## Resume procedure

1. Read `MASTER_PLAN.md`, this file, `ROADMAP.md`, `ARCHITECTURE.md`, `DECISIONS.md`.
2. Inspect latest `main` SHA, changed files, relevant implementation and GitHub Actions checks; the documented snapshot may lag.
3. Choose one testable vertical slice or a regression fix; inspect its public contracts and implementation.
4. Make code + regression-test changes, run applicable builds/tests, and record exact scope and results.
5. Update this snapshot and the roadmap **only for verified changes**; do not equate CI compilation with real GPU/device acceptance. Preserve detailed older progress in `history/`.

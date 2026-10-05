# NEngine Master Plan

This document is the single high-level reference for the product goal, scope, implementation order, completion criteria, and intentionally excluded targets.

For exact implementation status, also read `docs/CURRENT_STATE.md`.
For milestone checkboxes, also read `docs/ROADMAP.md`.
For public compatibility targets, also read `docs/API_SPEC.md`.
For architectural rules and accepted decisions, also read `docs/ARCHITECTURE.md` and `docs/DECISIONS.md`.

## 1. Product goal

NEngine is an independent clean-room game engine designed for developers who already understand Unity-style workflows.

The 1.0 goal is:

> A Unity-experienced developer can create and ship ordinary 2D and general-purpose 3D Windows/Android games without learning a fundamentally different object model or workflow.

NEngine targets **developer knowledge compatibility**, not Unity project or binary compatibility.

The engine should feel familiar in concepts such as:
- GameObject / Component style composition.
- Transform hierarchy.
- Scene and Prefab workflows.
- Inspector-driven serialized properties.
- Play Mode with an isolated runtime World.
- C# gameplay scripting.
- Camera / Mesh / Material / Light rendering concepts.
- Rigidbody / Collider / trigger/query physics workflows.
- Canvas-style runtime UI.
- Visual Studio-centered scripting/debugging.

NEngine does **not** aim to:
- load arbitrary Unity projects directly.
- execute Unity binaries.
- reproduce Unity source code.
- provide drop-in `.unitypackage` compatibility.
- clone every Unity package or every advanced rendering feature.

## 2. Supported product platforms

### 1.0 targets
- Editor: Windows x64.
- Player: Windows x64.
- Player: Android.
- Graphics: Vulkan-first on Windows and Android.
- Gameplay language: C#.
- Primary IDE: Visual Studio.
- Additional IDE integration may include Rider and VS Code later.

### Outside the 1.0 target
- iOS.
- macOS.
- Web.
- XR.
- Console platforms.
- Full HDRP parity.
- Full VFX Graph parity.
- DOTS/Burst parity.

A Windows D3D12 renderer may be considered after 1.0 if compatibility or performance QA justifies it.

## 3. Core product principles

### Familiar outside, independent inside
The public workflow should be Unity-familiar, while the internal implementation is independently designed and may use data-oriented storage.

### Same World for Editor and Player
Editor and runtime use the same object/component model. Play Mode clones editor state into a runtime World and discards runtime-only changes when stopped.

### Stable asset identity
Assets use persistent GUID + metadata identity rather than filesystem paths as runtime identity.

### Vulkan-first renderer
Windows and Android share one primary rendering architecture to control complexity.

### C# gameplay
C# is the main gameplay language. Generated `.sln` and `.csproj` files are IDE/build artifacts, not authoritative engine state.

### External SDKs stay outside core
AdMob, Firebase, Steam, analytics and similar SDKs belong in packages/adapters behind stable plugin contracts.

### Toolchains are replaceable modules
Android JDK/SDK/NDK/Gradle/AGP combinations are versioned side-by-side profiles rather than hardwired into the editor.

## 4. Major engine subsystems

NEngine is intentionally organized around a limited number of first-class subsystems:

1. Core / memory primitives
2. Object World
3. Serialization / Scene / Prefab
4. Assets / import database
5. Scripting / C# host / IDE integration
6. Renderer / RHI
7. Physics 3D
8. Physics 2D
9. Animation
10. Audio
11. Input
12. Runtime UI
13. Navigation
14. Jobs
15. Editor
16. Build
17. Platform
18. Profiler / diagnostics
19. Package Manager / Plugin Runtime
20. Toolchain Manager

Dependency direction should remain one-way:

`Editor -> public engine API -> World -> runtime subsystems -> Assets/Jobs -> Platform`

Lower layers must not depend on editor or product-specific SDKs.

## 5. Target gameplay/API surface

### Object and scene model
- GameObject/Component-style workflow.
- Transform parent/child hierarchy.
- Active/enabled state.
- Scene sync/async load.
- Additive Scene subset.
- Prefab instances and overrides.
- Nested Prefab subset.
- Scriptable serialized data assets.

### C# lifecycle
Unity-familiar phases such as:
- Awake.
- Enable/Disable.
- Start.
- FixedUpdate.
- Update.
- LateUpdate.
- Destroy.

Exact naming may differ where necessary, but the mental model should remain familiar.

### Rendering
- Camera.
- MeshRenderer.
- Mesh.
- Material.
- Texture.
- Shader.
- Lights.
- Sprites.
- Tilemaps.
- Depth-tested 3D rendering.
- Mainstream PBR-class material path.
- Shadows.
- Particle system.
- Compact visual shader subset later.

### Physics
- Rigidbody-style 3D and 2D bodies.
- Common colliders.
- Collision/trigger callbacks.
- Raycasts and common spatial queries.
- Common joints where practical.

### Input
- Keyboard/mouse.
- Gamepad.
- Touch.
- Simple legacy-style convenience access.
- Action-map style input.

### Audio
- Audio clips.
- Sources.
- Listener.
- Mixer basics.
- Runtime decode/streaming as needed.

### Animation
- Animation clips.
- State-machine controller.
- Parameters.
- Blend-tree subset.

### Runtime UI
- Canvas-style hierarchy.
- RectTransform-like layout model.
- Text/images/buttons and common controls.
- Event routing and input integration.

### Navigation
- NavMesh-style baking/runtime data.
- Agent.
- Obstacle.
- Link subset.

### Utilities
- Math.
- Time.
- Debug/logging.
- Coroutine/timer scheduler.
- Profiler/diagnostics.

## 6. Asset pipeline target

The asset system should support:
- GUID + `.meta` identity.
- Importer registry.
- change detection.
- dependency graph.
- import cache.
- validated cached artifacts.
- texture decoding/transcoding.
- audio metadata and runtime decoding.
- glTF mesh/material cooking as the first complete model path.
- OBJ/FBX support later where practical.
- SPIR-V import.
- GLSL/HLSL -> SPIR-V toolchain.
- material assets.
- script assets.
- scene and prefab assets.

Runtime rendering should consume cooked/cache artifacts rather than reparsing source assets every frame.

## 7. Package/plugin target

Planned package contribution types:
- NuGet packages.
- managed DLLs.
- Windows x64 native DLLs.
- Android AAR.
- Android JAR.
- Android native SO.
- Maven dependencies.
- Android manifest contributions.
- Gradle contributions.
- permissions/features.
- adapter SDKs for third-party services.

Unity-specific plugins may require NEngine-specific adapters.

## 8. Editor target

The Windows editor should ultimately provide:
- Hierarchy.
- Scene View.
- Game View.
- Inspector.
- Project/Assets browser.
- Console.
- docking/tab layouts.
- gizmos and selection.
- Undo/Redo.
- Prefab editing.
- Scene open/save.
- Play/Pause/Step/Stop.
- asset import feedback.
- C# solution generation/open.
- profiler.
- frame debugger.
- package manager.
- build settings.
- Android toolchain manager.

## 9. Build/product target

NEngine should eventually export:
- Windows x64 Player.
- Android APK.
- Android AAB.

Productization work includes:
- build profiles.
- application metadata.
- icons.
- signing hooks.
- Android SDK/NDK/JDK/Gradle profiles.
- dependency resolution.
- package contributions.
- installer/update support.

## 10. Milestone plan

### 0.1 — Core foundation
Goal: stable World, serialization and component foundations.

Includes:
- generational Entity/World lifetime.
- Transform hierarchy.
- component storage/type registry.
- reflection.
- Scene serialization.
- EntityReference/AssetReference.
- Prefab data model foundations.
- Play Mode World cloning.
- tests/CI.

Status: substantially complete.

### 0.2 — Windows editor foundation
Goal: a usable native editor shell around the core model.

Includes:
- Hierarchy / Inspector / Scene / Assets / Console.
- selection.
- property editing.
- Undo/Redo.
- Scene dirty/savepoint model.
- Play/Pause/Step/Stop.
- Scene selection/gizmo.
- persistent layout.
- Scene open/save.

Status: substantially complete; full freeform docking remains.

### 0.3 — Asset database
Goal: stable project and imported-asset identity/cache architecture.

Includes:
- project manifest/bootstrap.
- GUID/meta.
- importer registry.
- file watcher.
- import fingerprints.
- cache validation.
- dependency graph.
- source metadata importers.
- shader binary import.

Remaining major work:
- real texture decode/transcode.
- glTF mesh/material cooking.
- shader source compilation.

### 0.4 — Vulkan renderer
Goal: move from renderer bootstrap to real asset-backed Scene/Game rendering.

Implemented foundation includes:
- loader / instance / physical/logical device.
- Win32 surface.
- swapchain.
- image views/render pass/framebuffers.
- shared depth target.
- synchronization.
- Vulkan buffers.
- staging uploads.
- GPU meshes.
- shader modules.
- graphics pipelines.
- MVP push constants.
- indexed drawing.
- built-in Cube/Quad GPU cache.
- World Camera/MeshRenderer preview.
- GPU texture upload.
- sampler.
- material combined-image-sampler descriptor.
- material descriptor binding.

Immediate implementation order:
1. Diagnostic textured shader and real sampled-pixel preview.
2. Decoded-pixel texture cache keyed by AssetGuid.
3. First real glTF MeshData/material cooking path.
4. Imported mesh/material rendering in VK Preview.
5. Shader compiler toolchain.
6. Replace remaining GDI presentation where appropriate.
7. Vulkan Game View.
8. PBR/lights/shadows/sprites.
9. Android Vulkan surface.

### 0.5 — C# scripting + IDE
Goal: real gameplay scripting.

Includes:
- .NET host.
- NEngine managed API assemblies.
- gameplay assembly build/load.
- managed component discovery.
- lifecycle execution.
- compile/reload.
- debugger symbols/attach.
- coroutine/timer scheduler.
- NuGet runtime integration.

### 0.6 — Physics, input, audio
Goal: ordinary playable gameplay foundation.

Includes:
- 3D physics.
- 2D physics.
- input actions and convenience APIs.
- keyboard/mouse/gamepad/touch.
- audio source/listener/mixer.
- runtime audio decode/streaming.

### 0.7 — Animation, UI, navigation
Goal: mainstream game presentation and interaction systems.

Includes:
- animation clips/state machine/blend trees.
- runtime Canvas-style UI.
- NavMesh.
- particle system.
- smart-camera helpers.

### 0.8 — Packages/plugins
Goal: practical third-party integration.

Includes:
- managed/native plugin loading.
- NuGet.
- Windows DLL.
- Android AAR/JAR/SO/Maven.
- dependency resolution.
- manifest/Gradle contributions.
- adapter SDK.

### 0.9 — Build/toolchain/productization
Goal: create redistributable games rather than only running inside the editor.

Includes:
- Windows Player exporter.
- Android APK/AAB exporter.
- Android toolchain profiles.
- signing.
- profiler/frame debugger.
- installer/update/package cache.

### 1.0 — First complete product target
Completion means an experienced Unity developer can reasonably build and ship an ordinary 2D/general 3D Windows/Android game using NEngine without having to adopt a fundamentally different workflow.

## 11. Current development focus

The active development line is currently Renderer 0.4.

The immediate sequence is:

`descriptor binding -> textured sample path -> decoded texture asset cache -> glTF mesh/material cooking -> imported asset VK Preview -> shader toolchain`

After the renderer/resource boundary is proven with real assets, development returns to the .NET gameplay runtime.

## 12. Definition of done for important features

A feature is not considered complete merely because it compiles.

Where applicable, completion requires:
- Windows build success.
- Ubuntu build success for portable modules.
- automated tests.
- graceful failure diagnostics.
- real Windows runtime acceptance where GUI/Vulkan drivers are involved.
- documentation update.
- no hidden undeclared build dependency.

## 13. Project continuity rule

When resuming NEngine development in a new session:

1. Read `docs/MASTER_PLAN.md`.
2. Read `docs/CURRENT_STATE.md`.
3. Read `docs/ROADMAP.md`.
4. Inspect latest `main` commit.
5. Inspect latest CI result.
6. Continue from the first incomplete item in the active milestone unless a regression must be fixed first.

This keeps long-running development sessions aligned even when conversation context is unavailable.

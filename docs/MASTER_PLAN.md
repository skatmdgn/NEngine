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

The authoritative, line-item completion checklist is [ROADMAP.md](ROADMAP.md); current feature/QA distinctions are in [CURRENT_STATE.md](CURRENT_STATE.md). Do not infer a milestone is complete from a version label.

| Milestone | Goal | Status as of 2026-10-17 |
| --- | --- | --- |
| 0.1 Core | World, Scene, reflection/serialization, Prefab basis | Substantially implemented/tested; Prefab product workflow extends beyond baseline |
| 0.2 Editor | Native Windows panels, editing, Play/Undo/Save | Usable foundation; freeform docking and production Vulkan Game View pending |
| 0.3 Assets | GUID cache/import/dependencies | Implemented foundation; FBX, WebP/transcoding and other import parity pending |
| 0.4 Render | Vulkan on Windows + 2D/3D drawing | Vulkan Preview, materials, sprites implemented; production PBR/shadows/Game View/Android pending |
| 0.5 C# | Gameplay host, lifecycle, IDE project generation | Implemented foundation; packaged SDK, attach/debug symbols and binding parity pending |
| 0.6 Physics/Input/Audio | 2D/3D contact simulation, queries, WAV/input | Significant implementations and regression suites; advanced input/audio/solver QA pending |
| 0.7 Animation/UI/Nav | Sprite animation and authoring/runtime UI | Sprite + Canvas foundations present; general animation, production UI, NavMesh and particles pending |
| 0.8 Packages | Third-party managed/native/Android SDK integrations | Mostly not implemented |
| 0.9 Player/toolchains | Redistributable Windows + Android games | **Active, partial** Windows Player/export pipeline; Android exporter/toolchains absent |
| 1.0 Product | Ordinary 2D/3D Windows and Android shipping workflow | Not achieved |

## 11. Current development focus

**Development label: `0.9.0-dev`.** This denotes work underway on standalone Windows Player export/runtime integration, not successful completion of 0.1–0.9. Earlier subsystem milestones continue to receive fixes and production hardening.

Immediate integration path: **complete Player cooked imported-model/material subassets -> finish standalone managed input/property parity -> real Windows Vulkan/.NET/audio/input acceptance -> production Vulkan UI/gameplay render and plugin/package capabilities -> Android runtime/build/QA**.

Other outstanding work includes production PBR/shadows, compressed streaming audio, richer physics/collider/query tuning, nonlocal glTF asset policy, FBX/WebP, HLSL/reflection, navigation, particles, and advanced animation. Detailed scope and current blockers live in ROADMAP and CURRENT_STATE; avoid maintaining a second independent feature checklist here.

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

A new session must read `docs/MASTER_PLAN.md`, `docs/CURRENT_STATE.md`, `docs/ROADMAP.md`, `docs/ARCHITECTURE.md` and `docs/DECISIONS.md`, then inspect latest `main` commits, CI results and the relevant **actual source**. Documentation is a checkpoint, not proof of correctness. See [DEVELOPMENT_WORKFLOW.md](DEVELOPMENT_WORKFLOW.md) for the practical update protocol.

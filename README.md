# NEngine

NEngine is an independent, clean-room, Unity-familiar game engine project focused on **developer knowledge compatibility** rather than Unity project or binary compatibility.

Target scope:
- Windows editor
- Windows x64 player
- Android player
- 2D and general-purpose 3D
- C# gameplay scripting with Visual Studio/Rider integration
- Familiar GameObject / Component / Transform / Scene / Prefab workflows
- NuGet, managed DLL, Windows native DLL, and Android AAR/JAR/SO/Maven plugin paths

## Current milestone

**Status:** source and CI validated on 2026-10-17 KST at `main` `51db15f`. Windows + Ubuntu CI each passed all 11 CTest groups on that revision. This **does not** mean GPU/.NET end-to-end hardware QA or Android shipment is complete. See [CURRENT_STATE](docs/CURRENT_STATE.md).

`0.9.0-dev — standalone Windows Player/runtime/export integration in progress (pre-release, not feature-complete)`

The repository currently includes:
- generational Entity/World lifetime and Transform hierarchy
- type-erased native components and reflection metadata
- Scene v3 generic component serialization
- EntityReference and AssetReference persistence
- Prefab data/override foundations
- Play Mode World cloning
- Win32 editor shell with Hierarchy/Scene/Inspector/Assets/Console
- selection, Undo/Redo, Scene dirty tracking and persistent pane layout
- reflection-driven generic property editing
- GUID/.meta Asset Database, watcher, import cache and dependency graph
- texture/audio/model descriptors and validated SPIR-V shader import
- generated C# solution/project and NuGet manifest foundation
- hostfxr-based .NET runtime loading plus deterministic gameplay DLL/PDB builds
- ScriptBehaviour Awake/OnEnable/Start/FixedUpdate/Update/LateUpdate/OnDisable/OnDestroy execution in the cloned Play Mode World with fixed-step and host-frame scheduling split
- dedicated NEngine.API and stable NEngine.Bridge assemblies with collectible gameplay hot reload
- managed GameObject name/active/Transform hierarchy access plus native create/find/deferred-destroy through ABI v15
- managed Camera/Light, MeshRenderer/SpriteRenderer scalar+AssetGuid properties, and SpriteAnimator clip/playback/time controls through the generic PropertyAccessRegistry-backed native property bridge
- managed keyboard/mouse Input, global Time frame clock, coroutines and WaitForSeconds
- NEnginePhysics 3D/2D Rigidbody with linear/angular velocity integration and contact-island sleeping and wake propagation, rotated BoxCollider/SphereCollider/CapsuleCollider and BoxCollider2D/CircleCollider2D/CapsuleCollider2D, shared sweep-and-prune + OBB/radial/segment contacts, fixed-capacity reference/incident face-clipped box manifolds, collider-derived inertia with per-contact rotational sequential impulses and per-scene contact impulse warm starting, layer/mask filtering, managed collision/trigger callbacks, and rotation-aware raycast/overlap-box/BoxCast queries across box, radial and capsule targets
- NEngineAudio AudioSource/AudioListener data + Editor/C# bindings, deterministic playback clock, hierarchy/listener-oriented spatial stereo mixing, cached WAV PCM/float decode, software stereo rendering, and Windows WASAPI shared-mode output
- Camera/Light/MeshRenderer components
- RenderSnapshot with resolved world matrices
- camera matrix math and stable built-in Cube/Quad mesh AssetGuids
- dynamic Vulkan loader, instance, device, Win32 surface and swapchain
- swapchain image views, shared depth target, color+depth render pass, framebuffers and acquire/submit/present synchronization
- host-visible and staged device-local Vulkan buffers
- per-device built-in GPU mesh cache
- SPIR-V shader-module resources
- MeshVertex graphics pipeline with MVP push constants and depth testing
- multi-draw indexed submission
- GPU RGBA8 texture upload, sampler and material descriptor binding
- PNG/JPEG decoding through pinned stb_image plus BMP/TGA decode foundation
- .nmat Material AssetGuid assets with texture dependency tracking and Vulkan descriptor caching
- texture-sampling diagnostic shaders exercised by VK Preview
- glTF/GLB geometry decode into MeshData with AssetGuid CPU/GPU mesh caches, including selected-scene node hierarchy TRS/matrix baking and mirrored winding correction
- opt-in VK Preview that renders the actual presentation World Camera + built-in/imported glTF geometry, explicit .nmat textures, and auto-sampled glTF first-primitive PBR base-color PNG/JPEG or color-only baseColorFactor when MeshRenderer.material is unset
- external multi-file .gltf BIN/PNG/JPEG sidecar staging, import-fingerprint invalidation and watcher-driven dependency reimport
- OBJ/MTL import, SpriteRenderer/SpriteAnimation, multi-slot PBR material cooking and GLSL/SPIR-V fixture tooling
- Windows + Ubuntu CI tests

The standalone Windows Player can export a validated project bundle, perform fixed-step physics, WAV/sprite updates and (when a compatible .NET host and prebuilt managed assemblies are available) invoke managed gameplay callbacks. Cooked textures and model/material descriptors have been added, but generated imported-model material subassets, complete input/property parity, real-world hardware acceptance, production Vulkan UI, third-party plugin deployment and Android APK/AAB export remain incomplete.

The Win32 Scene View still uses GDI by default for interactive object/gizmo editing. The opt-in VK Preview now uses the real Vulkan graphics path for depth-tested indexed World rendering, while the GDI view remains the safe editing fallback.

The .NET gameplay path is now live: the editor can build C# gameplay assemblies, host them through hostfxr, run ScriptBehaviour instances in Play Mode, and reload collectible gameplay assemblies without restarting the host runtime. The managed API is still a development surface rather than a packaged/versioned SDK, and broader component/lifecycle/debugger coverage remains.

This repository intentionally does not use Unity source code or Unity-source-derived implementation.

## Build

```bash
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

On single-config generators such as Ninja/Unix Makefiles, the `-C Release` argument to `ctest` is optional.

Start with `docs/MASTER_PLAN.md`, then read `docs/CURRENT_STATE.md`, `docs/ROADMAP.md` and `docs/DEVELOPMENT_WORKFLOW.md`. **For user-requested 8/10/15-덩어리 development, follow [BATCH_DEVELOPMENT.md](docs/BATCH_DEVELOPMENT.md): checkpoint code on a branch without an open PR, then run GitHub CI at the final PR gate.** Old status-log detail is archived under `docs/history/`.

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

`0.3.0-dev — Asset pipeline is integrated with the first concrete Vulkan runtime/GPU resource path`

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
- texture-sampling diagnostic shaders exercised by VK Preview
- opt-in VK Preview that renders the actual presentation World Camera + supported built-in MeshRenderer items
- Windows + Ubuntu CI tests

The Win32 Scene View still uses GDI by default for interactive object/gizmo editing. The opt-in VK Preview now uses the real Vulkan graphics path for depth-tested indexed World rendering, while the GDI view remains the safe editing fallback.

C# project generation exists, but the embedded .NET runtime/compile/reload path is **not implemented yet**.

This repository intentionally does not use Unity source code or Unity-source-derived implementation.

## Build

```bash
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

On single-config generators such as Ninja/Unix Makefiles, the `-C Release` argument to `ctest` is optional.

Start with `docs/MASTER_PLAN.md` for the product goal and full development plan, then read `docs/CURRENT_STATE.md` and `docs/ROADMAP.md` before doing project work.

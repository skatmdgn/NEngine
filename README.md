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
- camera matrix math and built-in Cube/Quad CPU meshes
- dynamic Vulkan loader, instance, device, Win32 surface and swapchain
- swapchain image views, render pass, framebuffers and acquire/submit/present synchronization
- host-visible and staged device-local Vulkan buffers
- GPU vertex/index mesh resources
- SPIR-V shader-module resources
- Windows + Ubuntu CI tests

The Win32 Scene View still uses GDI for visible diagnostic object/gizmo drawing. Vulkan now has a real window context and render-pass clear/present path, but the graphics-pipeline/indexed-draw path is still in progress.

C# project generation exists, but the embedded .NET runtime/compile/reload path is **not implemented yet**.

This repository intentionally does not use Unity source code or Unity-source-derived implementation.

## Build

```bash
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

On single-config generators such as Ninja/Unix Makefiles, the `-C Release` argument to `ctest` is optional.

See `docs/CURRENT_STATE.md` before doing project work.

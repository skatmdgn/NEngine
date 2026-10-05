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

`0.3.0-dev — Asset database complete enough for renderer integration; 0.4 renderer foundation in progress`

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
- texture/audio/model import descriptors
- generated C# solution/project and NuGet manifest foundation
- render Camera/Light/MeshRenderer components
- RenderSnapshot and RHI contracts
- Windows + Ubuntu CI tests

The current Scene View is still a GDI diagnostic/editor viewport. The Vulkan runtime backend, real GPU resources, shader system and draw submission are **not implemented yet**.

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

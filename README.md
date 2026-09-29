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
`0.1.x — Core foundation`

The repository currently contains the first production-oriented core: generational entity handles, an object world, transform hierarchy rules, versioned scene serialization, component type registration, tests, CI, and architecture/state documentation.

This repository intentionally does not use Unity source code or Unity-source-derived implementation.

## Build
```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

See `docs/CURRENT_STATE.md` before doing any project work.

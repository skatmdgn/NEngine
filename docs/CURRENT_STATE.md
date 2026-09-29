# Current state

Version: 0.2.0-dev
Milestone: Windows editor foundation

## Verified locally
- [x] CMake project builds on Linux with C++20.
- [x] Generational 64-bit entity handles.
- [x] Object World create/destroy/name/active state.
- [x] Transform component storage and hierarchy safety.
- [x] Stable component type registry and reflection metadata foundation.
- [x] Type-erased native component pools with typed access.
- [x] Deep World cloning, including component pools.
- [x] Prefab template data model and override-patch representation with validation.
- [x] Versioned `.nscene` object/Transform serialization with scene-local IDs.
- [x] Transactional scene loading.
- [x] Core unit tests.
- [x] Cross-platform editor-model tests.
- [x] Editor Selection model.
- [x] Undo/Redo command stack.
- [x] Play/Pause/Step model backed by cloned runtime World.
- [x] Platform window contract with Win32 implementation and non-Windows build stub.
- [x] CI definition for Ubuntu and Windows.

## Important serialization boundary
The current scene serializer persists object metadata, hierarchy and Transform. Generic native/C# component property serialization adapters are intentionally still pending.

## 0.1 Core foundation
- [x] Entity/World/Transform lifetime foundation.
- [x] Component storage/reflection metadata foundation.
- [x] Scene core serialization and transactional loading.
- [x] Prefab/override data representation.
- [x] World deep clone for Play Mode.
- [ ] Generic component serialization adapter contract (to be completed as built-in components and scripting land).

## 0.2 Windows editor foundation
- [x] Cross-platform editor model separated from presentation.
- [x] Selection model with stale-entity sanitization.
- [x] Undo/Redo foundation.
- [x] Play/Pause/Step state model.
- [x] OS window abstraction.
- [x] Win32 native window host implementation.
- [ ] Dockable editor presentation (Hierarchy / Inspector / Scene / Assets / Console).
- [ ] Property inspector binding to reflection metadata.
- [ ] Scene-view presentation and gizmo interaction.
- [ ] Native file dialogs/project open-save flow.

## Not started
- Assets/import database
- C# host/compilation/hot reload/debug attach
- Vulkan renderer
- Physics 3D/2D
- Animation
- Audio
- Input actions
- Runtime UI
- Navigation
- Particles
- Profiler/frame debugger
- Package/plugin manager
- Windows player exporter
- Android exporter/toolchain manager

## QA status
Linux local build/tests pass. Windows CI compiles/tests the Windows-specific source, but a CI build is not a substitute for interactive Windows GUI/GPU QA.

## Historical prototype
A previous disposable Win32 prototype proved that a Windows x64 PE editor executable could be produced. It is not the production architecture and is not the source of truth.

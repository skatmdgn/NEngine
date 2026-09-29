# Current state

Version: 0.1.3-dev
Milestone: Core foundation

## Verified locally
- [x] CMake project builds on Linux with C++20.
- [x] Generational 64-bit entity handles.
- [x] Object World create/destroy/name/active state.
- [x] Transform component storage.
- [x] Parent/child hierarchy with cycle rejection.
- [x] Child detachment when parent is destroyed.
- [x] Entity slot reuse without stale-handle aliasing.
- [x] Stable component type registry with collision/duplicate rejection.
- [x] Reflection metadata foundation with property kinds/flags.
- [x] Type-erased native component pool architecture with typed access.
- [x] Transform migrated onto the same component storage model as future native components.
- [x] Deep World cloning, including component pools, for future Play Mode snapshots.
- [x] Prefab template data model and typed override-patch representation with validation.
- [x] Versioned `.nscene` scene data format.
- [x] Scene capture, deterministic text serialization and restore.
- [x] Scene parent references use scene-local IDs rather than runtime entity handles.
- [x] Scene loading is transactional: malformed/cyclic data cannot partially replace the destination World.
- [x] Core unit test executable.
- [x] CI definition for Ubuntu and Windows.

## Started but not product-ready
- [ ] Native Windows editor shell.

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

## Historical prototype
A previous disposable Win32 prototype proved that a Windows x64 PE editor executable, hierarchy/inspector interactions, simple scene persistence, and generated Visual Studio project files could be produced. It is intentionally not the production architecture and is not the source of truth.

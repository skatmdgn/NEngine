# Current state

Version: 0.2.1-dev
Milestone: Windows editor foundation

## Verified
- [x] Linux local C++20 build and tests.
- [x] GitHub CI on Ubuntu and Windows for the previous native Win32 host checkpoint.
- [x] Generational Entity / World lifetime and hierarchy safety.
- [x] Type-erased native component pools with typed access.
- [x] Component registry and reflection property metadata foundation.
- [x] Deep World cloning for Play Mode.
- [x] Versioned object/Transform scene serialization with transactional loading.
- [x] Prefab template and override-patch data representation.
- [x] Editor Selection model.
- [x] Undo/Redo command stack, including name/active/Transform edits.
- [x] Play/Pause/Step editor state backed by a cloned Runtime World.
- [x] Platform window contract and Win32 native window backend.
- [x] Hierarchy presentation model generated from the World hierarchy.
- [x] Inspector presentation model driven by Component Registry metadata.
- [x] Toolbar presentation state driven by PlaySession and CommandStack.

## Important serialization boundary
The scene serializer currently persists object metadata, hierarchy and Transform. Generic native/C# component property serialization adapters remain pending and are tracked explicitly.

## 0.2 remaining
- [ ] Win32 editor shell binds Hierarchy/Inspector/Toolbar view models to actual controls.
- [ ] Scene View surface and gizmo interaction.
- [ ] Assets and Console panels.
- [ ] Native file dialogs/project open-save flow.
- [ ] Property-edit binding from Inspector controls back into Commands.
- [ ] Docking/layout persistence.

## Not started
- Asset/import database
- C# host/compile/hot reload/debug attach
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
- Windows Player exporter
- Android exporter/toolchain manager

## QA rule
CI compilation is not interactive GUI/GPU QA. Windows UI behavior and later Vulkan behavior require an actual Windows run before being marked runtime-verified.

# Current state

Version: 0.2.2-dev
Milestone: Windows editor foundation

## Verified checkpoints
- [x] Previous native Win32 host checkpoint passed GitHub CI on Ubuntu and Windows.
- [x] Linux local C++20 tests for Core and Editor Model passed before repository integration.
- [x] Entity/World lifetime and hierarchy safety.
- [x] Type-erased native component pools with typed access.
- [x] Component registry and reflection property metadata foundation.
- [x] Deep World cloning for Play Mode.
- [x] Versioned object/Transform scene serialization with transactional loading.
- [x] Prefab template and override-patch data representation.
- [x] Editor Selection model.
- [x] Undo/Redo commands for name, active state and Transform.
- [x] Play/Pause/Step model backed by cloned Runtime World.
- [x] Platform window contract and Win32 native window backend.
- [x] Hierarchy presentation model generated from World hierarchy.
- [x] Reflection-driven Inspector presentation model.
- [x] Toolbar presentation model.
- [x] Win32 shell now binds toolbar, hierarchy selection and inspector editing to EditorModel commands.
- [x] Play Mode presentation switches to the cloned Runtime World and returns to Edit World on Stop.
- [x] Basic GDI Scene View preview exists as a temporary presentation surface while preserving the permanent Scene/EditorModel boundary.

## Current Win32 editor behavior
The Windows shell contains:
- Undo / Redo
- Play / Pause-Resume / Step / Stop
- Hierarchy list with parent depth and selection
- Inspector name + active state
- Transform position/quaternion/scale editing through Undoable Commands
- Scene preview showing object positions in a top-down diagnostic view
- Console panel placeholder

The GDI Scene View drawing backend is explicitly temporary. Selection/Hierarchy/Inspector/Command/Play models are production architecture and remain when Vulkan replaces the viewport surface.

## Important serialization boundary
The scene serializer currently persists object metadata, hierarchy and Transform. Generic native/C# component property serialization adapters remain pending.

## 0.2 remaining
- [ ] Windows runtime interaction QA for the new shell.
- [ ] Scene gizmo interaction.
- [ ] Assets panel backed by the future Asset Database.
- [ ] Structured Console/log model.
- [ ] Native file dialogs/project open-save flow.
- [ ] Generic reflection-to-command property editing beyond Transform.
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
CI compile/test is not interactive GUI/GPU QA. A feature is only marked Windows-runtime-verified after actual execution on Windows.

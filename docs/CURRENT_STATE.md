# Current state

Version: 0.2.3-dev
Milestone: Windows editor foundation

## Verified checkpoints
- [x] Core/Editor architecture and cross-platform EditorModel tests established.
- [x] Earlier native Win32 host checkpoint passed GitHub CI on Ubuntu and Windows.
- [x] Entity/World, component pools, reflection metadata, Scene and Prefab foundations.
- [x] Selection, Undo/Redo, Play/Pause/Step.
- [x] Hierarchy/Inspector/Toolbar presentation models.
- [x] Win32 shell binds hierarchy selection and inspector editing through Commands.
- [x] Native Win32 Scene Open/Save workflow implemented through Core SceneSerializer.
- [x] UTF-8 engine strings <-> UTF-16 Win32 UI conversion is explicit.
- [x] Scene file API uses std::filesystem::path for Unicode-capable Windows paths.
- [x] CI matrix does not fail-fast; Windows compile always runs.
- [x] Successful Windows CI builds are configured to publish NEngineEditor.exe artifacts.

## Current Win32 editor behavior
- Open / Save scene
- Undo / Redo
- Play / Pause-Resume / Step / Stop
- Hierarchy list with parent depth, active marker and selection
- Inspector name + active state
- Transform position/quaternion/scale edits through Undoable Commands
- Diagnostic top-down Scene View
- Console placeholder/log lines

The diagnostic GDI Scene View is temporary presentation only. The permanent EditorModel/World/Command/Inspector boundaries remain when Vulkan replaces it.

## Important serialization boundary
The Scene serializer currently persists object metadata, hierarchy and Transform. Generic native/C# component property serialization adapters remain pending.

## 0.2 remaining
- [ ] Windows runtime interaction QA for the shell.
- [ ] Scene selection/gizmo interaction.
- [ ] Assets panel backed by Asset Database.
- [ ] Structured Console/log model.
- [ ] Generic reflection-to-command editing beyond Transform.
- [ ] Docking/layout persistence.

## QA rule
CI build/test is not interactive GUI/GPU QA. Windows UI behavior is marked runtime-verified only after actual execution.

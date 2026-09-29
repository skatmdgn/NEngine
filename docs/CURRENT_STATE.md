# Current state

Version: 0.2.3-dev
Milestone: Windows editor foundation

## Verified checkpoints
- [x] Core/Editor model architecture established and tested.
- [x] Previous native Win32 host checkpoint passed GitHub CI on Ubuntu and Windows.
- [x] Entity/World lifetime, hierarchy safety, component pools and reflection metadata.
- [x] Deep World cloning for Play Mode.
- [x] Versioned object/Transform scene serialization with transactional loading.
- [x] Prefab template and override-patch data representation.
- [x] Selection, Undo/Redo, Play/Pause/Step.
- [x] Hierarchy/Inspector/Toolbar presentation models.
- [x] Win32 shell binds hierarchy selection and inspector edits through Commands.
- [x] Win32 Open/Save scene workflow uses native file dialogs.
- [x] Scene file APIs accept filesystem paths so Windows Unicode paths are representable at the Core boundary.
- [x] CI matrix is fail-fast disabled; Windows compile runs even if another OS fails.
- [x] Successful Windows CI builds publish NEngineEditor.exe as an artifact.

## Current Win32 editor behavior
- Open / Save scene
- Undo / Redo
- Play / Pause-Resume / Step / Stop
- Hierarchy list with parent depth and selection
- Inspector name + active state
- Transform position/quaternion/scale editing through Undoable Commands
- Scene preview showing object positions in a top-down diagnostic view
- Console placeholder/log lines

The GDI Scene View backend is temporary. The permanent World/Selection/Inspector/Command/Play boundaries are not.

## Important serialization boundary
The scene serializer currently persists object metadata, hierarchy and Transform. Generic native/C# component property serialization adapters remain pending.

## 0.2 remaining
- [ ] Windows runtime interaction QA for the shell.
- [ ] Scene gizmo interaction.
- [ ] Assets panel backed by the future Asset Database.
- [ ] Structured Console/log model.
- [ ] Generic reflection-to-command property editing beyond Transform.
- [ ] Docking/layout persistence.

## QA rule
CI compile/test is not interactive GUI/GPU QA. A feature is only marked Windows-runtime-verified after actual execution on Windows.

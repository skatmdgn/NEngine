# Current state

Version: 0.2.5-dev
Milestone: Windows editor foundation

## Verified checkpoints
- [x] Core/Editor architecture and cross-platform EditorModel tests established.
- [x] Native Win32 host compiles on GitHub Windows CI.
- [x] Entity/World, component pools, reflection metadata, Scene and Prefab foundations.
- [x] Selection, Undo/Redo, Play/Pause/Step.
- [x] Hierarchy/Inspector/Toolbar presentation models.
- [x] Native Win32 Scene Open/Save workflow through Core SceneSerializer.
- [x] UTF-8 engine strings <-> UTF-16 Win32 UI conversion.
- [x] Scene file API uses std::filesystem::path for Unicode-capable Windows paths.
- [x] Windows editor builds as a GUI subsystem executable with statically linked MSVC runtime.
- [x] Startup diagnostics write to %LOCALAPPDATA%/NEngine/Logs/editor.log.
- [x] Previous 0.2.4 Windows build/test/artifact generation succeeded in CI.

## 0.2.5 runtime-stability change
The user reported that 0.2.4 created the native top-level window but exited during Win32EditorShell::attach().

0.2.5 removes the risky top-level WNDPROC subclassing path entirely:
- the platform-owned main window is no longer modified by Editor UI code;
- a dedicated NEngine.EditorHost child window owns all editor controls;
- the host is resized from the main loop via Win32EditorShell::tick();
- attach() writes fine-grained progress markers for class registration, host creation, toolbar, hierarchy, Scene View, Inspector, Console and initial layout;
- the startup log is truncated on each run so the latest run is unambiguous.

## Current Windows editor behavior
- Open / Save scene
- Undo / Redo
- Play / Pause-Resume / Step / Stop
- Hierarchy selection
- Inspector name + active state
- Transform edits through Undoable Commands
- Diagnostic top-down Scene View
- Console placeholder/log lines

## Runtime QA status
- 0.2.3: user reported brief window + console, then exit.
- 0.2.4: user reported immediate exit; log proved main Win32 window creation succeeded and failure occurred inside shell attach.
- 0.2.5: pending CI and user-side runtime verification.

## Important serialization boundary
Scene serialization currently persists object metadata, hierarchy and Transform. Generic native/C# component property serialization adapters remain pending.

## 0.2 remaining
- [ ] Windows runtime confirmation of 0.2.5 startup.
- [ ] Scene selection/gizmo interaction.
- [ ] Assets panel backed by Asset Database.
- [ ] Structured Console/log model.
- [ ] Generic reflection-to-command editing beyond Transform.
- [ ] Docking/layout persistence.

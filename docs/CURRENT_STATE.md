# Current state

Version: 0.2.4-dev
Milestone: Windows editor foundation

## Verified checkpoints
- [x] Core/Editor architecture and cross-platform EditorModel tests established.
- [x] Native Win32 host compiles on GitHub Windows CI.
- [x] Entity/World, component pools, reflection metadata, Scene and Prefab foundations.
- [x] Selection, Undo/Redo, Play/Pause/Step.
- [x] Hierarchy/Inspector/Toolbar presentation models.
- [x] Win32 shell binds hierarchy selection and inspector editing through Commands.
- [x] Native Win32 Scene Open/Save workflow through Core SceneSerializer.
- [x] UTF-8 engine strings <-> UTF-16 Win32 UI conversion.
- [x] Scene file API uses std::filesystem::path for Unicode-capable Windows paths.
- [x] Windows CI artifacts are produced from commit-pinned builds.
- [x] Windows editor builds as a GUI subsystem executable rather than a console executable.
- [x] MSVC runtime is statically linked for portable CI artifacts.
- [x] Startup diagnostics write to %LOCALAPPDATA%/NEngine/Logs/editor.log.
- [x] Startup failures surface through MessageBox instead of disappearing silently.
- [x] Commit 25b18446071938158ae786a287510be20eaf3498 passed Ubuntu and Windows build/tests and published NEngineEditor-windows-x64.

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
The previous 0.2.3 artifact was reported by the user to briefly show a window and console, then exit.
The 0.2.4 artifact removes the console subsystem, statically links the MSVC runtime, and adds startup logging/error dialogs.
Actual user-side runtime confirmation of 0.2.4 is still pending.

## Important serialization boundary
Scene serialization currently persists object metadata, hierarchy and Transform. Generic native/C# component property serialization adapters remain pending.

## 0.2 remaining
- [ ] Windows runtime confirmation of 0.2.4 startup.
- [ ] Scene selection/gizmo interaction.
- [ ] Assets panel backed by Asset Database.
- [ ] Structured Console/log model.
- [ ] Generic reflection-to-command editing beyond Transform.
- [ ] Docking/layout persistence.

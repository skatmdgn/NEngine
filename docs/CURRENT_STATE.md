# Current state

Version: 0.2.7-dev
Milestone: Windows editor foundation

## Windows runtime verified
The user has now verified the 0.2.6 editor on Windows:
- [x] Editor window remains open normally.
- [x] Hierarchy shows Main Camera / Cube / Child Cube.
- [x] Hierarchy selection updates Inspector.
- [x] Name editing works.
- [x] Position editing + Apply Transform works.
- [x] Undo / Redo work.
- [x] Play / Pause / Step / Stop work.
- [x] .nscene Save / Open round-trip works.
- [x] Korean object names such as 플레이어 display correctly.

This closes the 0.2.6 startup/runtime-stability checkpoint.

## 0.2.7 Scene View interaction
Implemented:
- [x] Platform-independent Scene View projection module.
- [x] Parent-aware diagnostic world-position calculation.
- [x] Scene View object picking.
- [x] X/Z translation gizmo hit-testing.
- [x] Screen drag delta -> local Transform conversion.
- [x] Win32 Scene View click-to-select.
- [x] Selected object draws X and Z translation axes.
- [x] Dragging an axis previews the Transform live.
- [x] Mouse release commits exactly one SetTransformCommand.
- [x] Undo / Redo therefore treat one drag as one edit.
- [x] Lost mouse capture restores the pre-drag Transform.
- [x] Scene interaction math has cross-platform unit coverage.

## Existing foundations
- Generational Entity / World lifetime.
- Parent/child Transform hierarchy safety.
- Type-erased native Component pools.
- Reflection metadata foundation.
- Scene and Prefab foundations.
- Selection and CommandStack.
- Play Mode cloned Runtime World.
- Reflection-driven Inspector presentation.
- Dedicated Win32 EditorHost child window.
- UTF-8 engine strings <-> UTF-16 Win32 UI.
- Native Scene Open / Save.
- Windows CI artifacts.

## Rendering boundary
The current Scene View still uses GDI as a diagnostic/editor presentation surface.
Projection, picking and gizmo interaction are platform-independent Editor code so the behavior survives the future Vulkan viewport replacement.

## Serialization boundary
Scene serialization currently persists object metadata, hierarchy and Transform.
Generic native/C# Component property serialization adapters remain pending.

## 0.2 remaining
- [ ] User-side Windows runtime verification of click-selection and X/Z gizmo dragging.
- [ ] Structured Console/log model.
- [ ] Asset Database + Assets panel.
- [ ] Generic reflection-to-command property editing beyond Transform.
- [ ] Docking/layout persistence.

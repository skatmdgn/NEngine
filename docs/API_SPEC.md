# Public API compatibility surface

This is a capability map, not a promise to duplicate every Unity symbol or implementation.

Legend: `A` familiar/direct, `B` equivalent, `C` reduced/alternative, `D` intentionally out of scope.

| Area | Grade | Target |
|---|---:|---|
| Object/GameObject/Component | A | familiar object/component workflow |
| Transform hierarchy | A | position/rotation/scale/parent/children |
| Lifecycle | A | Awake/Enable/Start/FixedUpdate/Update/LateUpdate-style phases |
| Math/Time/Debug | A | common gameplay surface |
| Scene | A | sync/async load, additive subset |
| Prefab | A/B | instances, overrides, nested subset, variants later |
| Scriptable data assets | A | serialized non-scene data objects |
| Coroutine/timers | A | yield/timer scheduler equivalent |
| Physics 3D/2D | A/B | rigidbodies, colliders, queries, triggers, common joints |
| Sprite/Tilemap | A | mainstream 2D workflow |
| Mesh/Material/Camera/Light | A/B | URP-class mainstream rendering target |
| Animator | B | state machine/parameters/blend trees subset |
| Runtime UI | A/B | Canvas/RectTransform-style mental model, one unified implementation |
| Input | A/B | legacy convenience plus action maps |
| Audio | A/B | clips/sources/listener/mixer basics |
| Navigation | B | NavMesh mainstream workflow |
| Smart camera | B | follow/orbit/TPS/FPS/rail/shake/blend |
| Particle | B | mainstream CPU/GPU particle features |
| Shader Graph | C | compact visual shader subset only |
| HDRP/VFX Graph/DOTS/Burst | C/D | not parity targets |
| XR/consoles/iOS/macOS/Web | D for 1.0 | outside agreed platform scope |
| Unity project/binary/plugin drop-in compatibility | D | explicitly not a target |

## Managed Canvas foundation (ABI v16)

The generated `NEngine.UI` class provides `Find(string nameOrLegacyText)`, `WasClicked(ulong id)`, `GetText/SetText`, `IsVisible/SetVisible`, and `IsInteractable/SetInteractable`. For Canvas v2, assign an optional unique stable widget `name` (up to 128 ASCII identifier characters); `Find` prefers that name, falling back to visible text for older unnamed widgets. Canvas v1 files remain readable; writing produces v2. All writes act on the Play Mode Canvas copy, not the imported source asset. Missing/non-Play UI data is unavailable and writes return false. `WasClicked` reports mouse/Enter/Space clicks through the next managed script frame, then clears. `UI.AddClickListener(ulong, Action)`, `RemoveClickListener(ulong, Action)`, and `ClearClickListeners()` provide frame-dispatched callbacks. Listeners are also cleared on managed Play reset, gameplay unload and UI-bridge shutdown so collectible C# game assemblies are not retained. Text updates are capped at 4096 UTF-8 bytes. Native Canvas edit hit-testing and bounded transactional Undo/Redo have been added as foundations; visual UI authoring/save integration and production Vulkan Game View/font rendering remain future work.

## Canvas authoring API (0.7)

Native `NEngineUI` provides `AnchorPreset` (Top/Middle/Bottom x Left/Center/Right, StretchHorizontal, StretchVertical, StretchAll) and `apply_anchor_preset(UiRect parent, UiRect rect, AnchorPreset)`, which preserve the rectangle at a specific preview size. `EditorModel::ui_edit_apply_anchor_preset` validates the selection and commits the change to Canvas Undo history. `toggle_ui_widget` maintains a multi-widget selection separate from the primary inspector widget; group drags and group subtree deletions commit once. `ui_edit_reparent_preserve_geometry(parent,width,height)` rejects hierarchy cycles and keeps pixel geometry when moving the selected widget under a new parent. This is native editor functionality, not a managed runtime API or a complete WYSIWYG Unity UI replacement.

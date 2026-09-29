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

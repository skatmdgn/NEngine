# Roadmap

Roadmap entries are implementation order, not promises tied to dates.

## 0.1 — Core foundation
- [x] Entity/World lifetime
- [x] Stable component type registry
- [x] Transform hierarchy
- [x] Scene data model and versioned serializer
- [x] Generic component storage/reflection
- [x] Generic component Scene codecs
- [x] EntityReference/AssetReference persistence
- [x] Prefab data model and override representation
- [x] World clone/snapshot for Play Mode
- [x] Tests and CI

## 0.2 — Windows editor foundation
- [x] Native window/application layer
- [x] Hierarchy / Inspector / Scene / Assets / Console shell
- [x] Selection and command stack
- [x] Undo/Redo
- [x] Play/Pause/Step using cloned World
- [x] Scene picking and translation gizmo
- [x] Generic reflection-driven property editing
- [x] Structured Console model
- [x] Persistent resizable pane layout
- [x] Scene dirty/savepoint tracking
- [ ] Full freeform docking/tab system

## 0.3 — Asset database
- [x] GUID/meta database
- [x] File watcher
- [x] Importer contract
- [x] Import fingerprint/cache manifest
- [x] Generated subasset manifest v3 with deterministic derived GUIDs
- [x] Stale root/subasset cache artifact pruning after reimport
- [x] Validated cache artifact lookup
- [x] Dependency graph
- [x] Project startup Scene/bootstrap
- [x] Asset activation routing in Editor
- [x] Texture metadata importer
- [x] WAV metadata importer
- [x] Model source descriptor importer
- [x] SPIR-V binary shader importer
- [x] Renderer cache resolver for texture/model/shader artifacts
- [x] PNG/JPEG RGBA8 decode with pinned stb_image
- [x] BMP/TGA RGBA8 decode foundation
- [x] Gamma-aware full RGBA8 mip-chain generation/upload
- [ ] WebP decode + texture compression/transcoding
- [x] .nmat importer + texture dependency graph
- [x] glTF/GLB geometry decode into MeshData
- [x] glTF selected-scene node hierarchy TRS/matrix baking into MeshData
- [x] glTF inverse-transpose normal transform + mirrored winding correction
- [x] glTF node-cycle/invalid-transform rejection
- [x] First glTF PBR base-color image decode (PNG/JPEG GLB bufferView/data URI/external file)
- [x] glTF baseColorFactor 1x1 color material and linear-space image tint
- [x] Reject unsafe external glTF resource URIs
- [x] AssetGuid decoded/GPU mesh caches
- [x] glTF referenced material slots -> stable generated .nmat/Texture subasset cooking
- [x] External .gltf BIN/image sidecar dependency staging with fingerprint + watcher reimport
- [x] Sidecar removal/restoration recovery with persistent .meta GUID
- [x] Percent-encoded local sidecar URIs + content-hash invalidation + recovery without .meta
- [ ] Explicit policy/support for nonlocal/outside-model-directory glTF resources
- [x] OBJ mesh decoding (v/vt/vn, negative indices, n-gon triangulation, generated normals)
- [x] OBJ MTL material cooking
- [ ] FBX mesh/material decoding/cooking
- [x] GLSL shader compile/embed toolchain + CI fixture validation
- [ ] HLSL shader compiler validation/toolchain parity

## 0.4 — Vulkan renderer
- [x] RHI contract
- [x] Camera/Light/MeshRenderer native components
- [x] RenderSnapshot extraction
- [x] Hierarchy-resolved render world matrices
- [x] Render component Inspector + Scene integration
- [x] Camera view/projection matrix math
- [x] CPU Cube/Quad MeshData
- [x] Vulkan loader/instance
- [x] Physical/logical device and graphics queue
- [x] Windows Vulkan surface
- [x] Swapchain creation/recreation
- [x] Swapchain image views/render pass/framebuffers
- [x] Acquire/submit/present synchronization
- [x] Render-pass clear frame
- [x] Vulkan host-visible/device-local buffers
- [x] Staging copy into device-local buffers
- [x] GPU vertex/index mesh resource
- [x] SPIR-V shader module resource
- [x] Graphics pipeline/layout
- [x] Indexed mesh draw command path
- [x] Opt-in Win32 Vulkan Preview
- [x] Depth buffer and depth-tested pipeline
- [x] Built-in mesh AssetGuid resolver and GPU mesh cache
- [x] Multi-draw World Camera/MeshRenderer Vulkan preview
- [x] GPU texture/sampler resource
- [x] Combined-image-sampler material descriptor resource
- [x] Graphics pipeline layout accepts material descriptor set layout
- [x] Bind material descriptor set during indexed draw
- [x] Textured fragment shader/sample path
- [x] AssetGuid decoded RGBA8 texture cache (BMP/TGA foundation)
- [x] Per-device Vulkan texture/material AssetGuid cache
- [x] Imported glTF/GLB mesh AssetGuid resolution in VK Preview
- [x] .nmat Material AssetGuid -> imported texture Vulkan binding
- [x] GLB/glTF per-primitive base-color texture automatic Vulkan preview fallback
- [x] GLB/glTF baseColorFactor image-free color auto preview
- [x] Multi-primitive/submesh ranged draws + per-material Vulkan descriptors
- [x] Automatic referenced glTF material/image -> stable generated NEngine .nmat/Texture subasset cooking
- [x] Cooked model material-map cache with direct-glTF fallback
- [ ] Scene View Vulkan presentation replacement
- [ ] Android Vulkan surface
- [x] SpriteRenderer Texture/PPU/sort/flip + alpha-blended Vulkan draw path
- [x] SpriteAnimation clip importer/cache + Play Mode frame animation
- [x] glTF PBR material v2 cooking (base/normal/metallic-roughness/emissive/occlusion + factors/alpha/double-sided)
- [x] PBR shader source/compiler fixture foundation
- [ ] Production lighting/shadows and full PBR validation

## 0.5 — C# scripting + IDE
- [x] hostfxr discovery/runtime loading foundation
- [x] generated sln/csproj foundation
- [x] NuGet package-reference manifest foundation
- [x] dotnet SDK discovery + gameplay DLL/PDB build
- [x] Editor Build C# workflow
- [x] ScriptBehaviour native component + Scene serialization + Inspector/Add Component
- [x] Managed Behaviour Create/Start/Update/OnDestroy lifecycle bridge
- [x] Play Mode ScriptBehaviour execution on cloned World
- [x] Native <-> managed Transform local position/rotation/scale synchronization
- [x] Dedicated generated NEngine.API + stable NEngine.Bridge assemblies
- [x] Safe collectible AssemblyLoadContext gameplay hot reload/unload
- [x] Native World callback ABI v14 for GameObject lifetime/name/active, Transform hierarchy/TRS, component properties and physics query injection
- [x] Managed GameObject create/find plus deferred self/foreign destroy
- [x] Managed keyboard/mouse Input API over native callback table
- [x] Managed Time.deltaTime/time/frameCount host-frame clock plus fixedDeltaTime fixed-step value
- [x] Coroutine/WaitForSeconds scheduler with nested IEnumerator and stop APIs
- [x] Awake lifecycle before first activation with native GameObject/Transform state already bound
- [x] OnEnable/OnDisable activation lifecycle with persistent disabled/inactive managed instances
- [x] LateUpdate second-pass ordering after all active managed Update calls
- [x] FixedUpdate fixed-step lifecycle plus host-frame Update/LateUpdate scheduling split
- [x] Generic native property ABI + managed Camera/Light and non-asset MeshRenderer/SpriteRenderer property bindings
- [x] ABI v14 UTF-8/AssetGuid transport + SpriteRenderer.texture + MeshRenderer.mesh/material bindings
- [x] Managed SpriteAnimator clip/enabled/playing/loop/speed/time bindings + Play/Pause/Stop/Restart helpers
- [x] Managed Rigidbody/BoxCollider + Rigidbody2D/BoxCollider2D component/property bindings
- [x] Managed Collision/Trigger Enter/Stay/Exit callbacks + Collision/Collision2D payloads
- [x] Managed Physics.Raycast + Physics2D.Raycast with layer masks over ABI v14 injected native queries
- [x] Managed Physics.OverlapBox + Physics2D.OverlapBoxAll multi-hit queries over ABI v14
- [x] Managed AudioSource/AudioListener properties + AudioSource Play/Pause/UnPause/Stop/isPlaying/runtime time
- [ ] Visual Studio attach/debug symbols

## 0.6 — Physics, input, audio
- [x] NEnginePhysics module + 3D/2D Rigidbody and BoxCollider metadata/Scene/Editor foundations
- [x] Fixed-step 3D/2D gravity + linear-velocity Transform integration foundation
- [x] Initial 3D AABB BoxCollider overlap + position contact resolution foundation
- [x] Initial 2D AABB BoxCollider2D overlap + XY contact resolution foundation
- [x] Native collision/trigger Enter/Stay/Exit tracker + OverlapBox/OverlapBox2D query foundation
- [x] Managed collision/trigger callbacks + 3D/2D raycast query APIs
- [x] Sweep-and-prune broad phase + collider layer/collision-mask contact filtering
- [x] Managed 3D/2D overlap-box queries + query layer-mask filtering
- [x] Native axis-aligned BoxCast/BoxCast2D foundation
- [ ] Managed cast bindings + richer hit semantics/query filtering policy
- [ ] Rotated/extra collider shapes, friction/restitution, sleeping and production broad-phase tuning
- [x] Keyboard/mouse InputState transitions + Win32 message capture
- [x] Named action-map binding/query foundation
- [x] Managed KeyCode/GetKey/GetKeyDown/GetKeyUp + mouse position/delta/wheel
- [ ] Gamepad/touch input backends and production action-map persistence
- [x] NEngineAudio AudioSource/AudioListener + deterministic playback clock + spatial stereo mix snapshot
- [x] Cached WAV PCM/float runtime decode + AssetGuid AudioClipCache
- [x] Software stereo renderer + Windows WASAPI shared-mode output + Play Mode device pump
- [x] Hierarchy/listener-oriented spatial pan + configurable min/max distance attenuation
- [ ] Streaming decode + OGG/MP3/FLAC runtime decode + production realtime device scheduling

## 0.7 — Animation/UI/navigation
- [ ] animation clips/state machine/blend tree subset
- [ ] runtime Canvas-style UI
- [ ] NavMesh agent/obstacle/link subset
- [ ] particle system

## 0.8 — Packages/plugins
- [ ] NuGet restore/runtime integration
- [ ] managed DLL
- [ ] Windows native DLL
- [ ] Android AAR/JAR/SO/Maven contributions
- [ ] dependency resolution
- [ ] adapter SDK

## 0.9 — Build/toolchain/productization
- [ ] Windows Player exporter
- [ ] Android APK/AAB exporter
- [ ] side-by-side Android toolchain profiles
- [ ] updater/package cache
- [ ] profiler/frame debugger
- [ ] installer/signing hooks

## 1.0 — First complete product target
A Unity-familiar developer can create and ship ordinary 2D/general 3D Windows/Android games without learning a fundamentally new object/workflow model.

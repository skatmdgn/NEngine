# Current state

Version: 0.5.0-dev
Milestone: C# gameplay scripting is active on top of the asset/Vulkan foundations.

## Verified baseline

The user previously verified the 0.2.6 Windows editor runtime:
- Editor opens and remains stable.
- Hierarchy / selection / Inspector work.
- Name and Transform editing work.
- Undo / Redo work.
- Play / Pause / Step / Stop work.
- Scene Save / Open round-trip works.
- UTF-8 names including Korean display correctly.

Newer 0.3/0.4/0.5 work is continuously built and tested on Windows + Ubuntu CI. A fresh user-side Windows acceptance pass is still required after the Vulkan/editor and managed-runtime changes.

## Core / Scene / Prefab

Implemented:
- Generational Entity handles and World lifetime.
- Parent/child Transform hierarchy with cycle prevention.
- Type-erased native component pools.
- Stable component type IDs and reflection metadata.
- World clone/snapshot for Play Mode.
- Scene v3 serialization.
- Generic native component serialization codecs.
- Scene-local EntityReference remapping across save/load.
- AssetReference serialization as GUID strings.
- Transactional Scene instantiate.
- Legacy Scene v1 read compatibility.
- Prefab model/override representation.
- Prefab structural validation independent of loaded runtime component codecs.
- Prefab EntityReference validation.

## Windows Editor

Implemented:
- Native Win32 editor shell.
- Hierarchy / Scene / Inspector / Assets / Console panes.
- Resizable Hierarchy / Inspector / bottom splitters.
- Persistent editor pane layout.
- Structured Console model with severity and duplicate collapse.
- Selection + command stack + Undo/Redo.
- Undo-aware Scene dirty/savepoint tracking.
- Dirty marker in editor title.
- Play Mode cloned Runtime World.
- Scene View click selection.
- X/Z translation gizmo drag committed as one command.
- Generic reflection-driven Inspector presentation.
- Generic property text editing for bool/int/uint/float/string/Vec3/Quaternion/EntityReference/AssetReference.
- Generic SetPropertyCommand path for arbitrary registered components.
- Scene Open / Save.
- Project startup Scene load.
- Asset double-click routing for Scene/script/external assets.
- C# solution generation/open workflow.
- Win32 Scene View attempts Vulkan window-context bootstrap.
- Vulkan bootstrap success/failure is reported to Console without making Vulkan availability an editor-startup requirement.

The Scene View still uses GDI by default for interactive diagnostic object/gizmo drawing. An opt-in **VK Preview** toolbar toggle now renders the actual presentation World through Vulkan when supported: it reads the active Camera plus MeshRenderer items from RenderSnapshot, resolves built-in Cube/Quad AssetGuids and supported imported glTF/GLB AssetGuids to cached GPU meshes, computes per-object MVP matrices, and submits multiple indexed draws in one render pass. Imported geometry uses an explicitly assigned .nmat texture when available; if material is unset, imported glTF/GLB submeshes first resolve persistent cooked material/texture subassets per primitive material slot, with the direct glTF PBR base-color decoder retained only as a fallback. Otherwise the renderer retains its diagnostic textured fallback. Turning the toggle off immediately returns to the GDI interaction view.

## Project / Asset database

Implemented:
- Persistent NEngine.nproject manifest.
- Safe project-relative startup Scene validation.
- Project bootstrap with Assets/Scenes, Assets/Scripts, Assets/Materials, ProjectSettings, Packages and Library/Cache.
- Persistent startup Main.nscene.
- Startup Scene contains native Camera and Light components.
- GUID + .meta asset identity.
- Extension importer registry.
- Polling file watcher.
- Automatic rescan and reimport for changed files.
- Import fingerprint/cache manifest v3.
- Deterministic generated subasset GUIDs, persistent generated artifact lists, lazy ProjectSession GUID recovery after reopen, and stale generated artifact pruning after reimport.
- Validated cache artifact lookup rejecting stale source/importer versions.
- Dependency graph forward/reverse edges, plus reimport of transitive dependents after file changes.
- Assets panel backed by AssetDatabase.
- Scene/script/raw source staging.
- Texture source staging + metadata descriptor.
  - PNG/BMP/TGA/JPEG dimension probing where supported.
- .nmat material importer with base-color Texture AssetGuid dependency extraction into the dependency graph.
- Audio source staging + WAV metadata descriptor.
  - channels, sample rate, bits/sample, data bytes.
- Model source staging + format/source-size descriptor.
- External .gltf sidecar BIN/image staging, AssetGuid dependency graph edges, content-hash import fingerprints, percent-decoded safe local URIs, and watcher-triggered parent reimport; deletion/restoration recovers the parent even when the sidecar .meta was lost.
- First glTF 2.0 geometry decode path:
  - .glb 2.0 JSON/BIN chunks.
  - .gltf base64 data-URI buffers.
  - external .gltf buffer/image sidecars copied into matching nested paths in the import cache, with sandboxed relative-URI checks.
  - TRIANGLES primitives.
  - float plus normalized BYTE/UNSIGNED_BYTE/SHORT/UNSIGNED_SHORT POSITION/NORMAL/TEXCOORD_0 attributes.
  - unsigned byte/short/int indices, including sparse index overlays.
  - sparse VEC2/VEC3 attribute overlays, including accessors with no base bufferView.
  - interleaved byteStride support.
  - multiple mesh primitives concatenated into MeshData while preserving submesh index ranges/material slots.
  - right-handed glTF -> NEngine left-handed Z reflection + winding conversion.
  - selected glTF scene node hierarchy baked into MeshData using parent/child world transforms.
  - node translation/rotation/scale and explicit column-major 4x4 matrix transforms.
  - inverse-transpose normal transformation plus mirrored-node winding correction.
  - repeated mesh node instances are baked as independent transformed geometry ranges.
  - cyclic/invalid node graphs and singular transforms are rejected diagnostically.
  - glTF external buffer/image path traversal and unsupported URI forms rejected.
- Referenced glTF material slots are cooked at import time into deterministic generated Texture + .nmat subassets; a persistent model material map binds submesh material slots back to those GUIDs.
- OBJ geometry decode with v/vt/vn, positive/negative face indices, n-gon fan triangulation, UV normalization, generated flat normals, bounds and shared MeshData/GPU-cache routing.
- OBJ usemtl/mtllib/map_Kd staging and MTL diffuse color/alpha/texture cooking into deterministic generated Texture + .nmat subassets.
- SPIR-V shader import.
  - .spv validation by size/magic.
  - .vert.spv/.frag.spv stage hints.
  - shader.nasset descriptor.
- Automatic import feedback in Console.

Not yet implemented:
- WebP pixel decoding and texture compression/transcoding policy (gamma-aware CPU mip generation + Vulkan mip upload are implemented).
- glTF skins and morph targets; current material cooking supports base color, normal, metallic-roughness, emissive, occlusion, factors, alpha mode/cutoff and double-sided metadata.
- Remote/nonlocal or outside-model-directory glTF resource policy/support; local percent-encoded sidecars are implemented.
- FBX decoding/cooking.
- HLSL compiler-path parity/validation; GLSL -> SPIR-V compile/embed tooling is implemented and exercised in CI.
- Audio decode/stream runtime.
- Dependency extraction from asset contents.

## C# / IDE foundation

Implemented:
- Managed project generator.
- .sln / .csproj generation.
- Assets/Scripts source inclusion.
- managed-packages.txt NuGet reference manifest.
- Visual Studio/Rider/default .sln association open path.
- hostfxr discovery/dynamic loading with generated runtimeconfig.
- dotnet SDK discovery, deterministic gameplay DLL/PDB build output and Editor **Build C#** action.
- Managed ABI v12 shared through stable NEngine.API/NEngine.Bridge assemblies.
- Native ScriptBehaviour component with Scene serialization, Add Component and generic Inspector editing.
- Play Mode ScriptBehaviour instance management against the cloned runtime World.
- Managed Behaviour Awake / OnEnable / Start / FixedUpdate / Update / LateUpdate / OnDisable / OnDestroy execution with managed instances preserved while disabled or inactive.
- Native <-> managed local Transform position/rotation/scale synchronization around lifecycle calls.
- Generated NEngine.API, stable NEngine.Bridge and gameplay projects are separate assemblies; collectible gameplay AssemblyLoadContext reloads user code without restarting hostfxr.
- Managed GameObject name/active state, Transform parent/children/TRS and built-in component presence query native World state.
- Managed GameObject construction and Find route through native World create/find callbacks.
- GameObject.Destroy uses an end-of-simulation-tick native destroy queue so self-destroy and foreign/duplicate destroy requests do not invalidate ScriptSystem iteration.
- Cross-platform keyboard/mouse Input callbacks expose KeyCode, GetKey/GetKeyDown/GetKeyUp, mouse position/delta and wheel.
- Managed Time.deltaTime/time/frameCount advances once per host Update frame regardless of Behaviour count; Time.fixedDeltaTime is supplied on each fixed simulation step without advancing the host frame clock.
- Coroutine scheduling supports StartCoroutine, StopCoroutine, StopAllCoroutines, nested IEnumerator, yield return null and WaitForSeconds.
- Generic ABI v12 native component property transport is backed by the Editor PropertyAccessRegistry; bool/integer/float/vector/quaternion/UTF-8 string values round-trip, with public managed AssetGuid support for SpriteRenderer.texture and MeshRenderer.mesh/material.
- Managed SpriteAnimator exposes enabled/clip/playing/loop/speed/runtime time plus Play/Pause/Stop/Restart helpers; Clip changes reset playback time and negative Time writes are rejected.

Not yet implemented:
- Packaged/versioned distribution of the NEngine managed API.
- Broader physics/audio component property APIs.
- Visual Studio debugger attach integration.
- NuGet runtime/package loading beyond generated project references.

## Renderer 0.4

### World/render data

Implemented:
- NEngineRender module.
- Native Camera component.
- Native Light component.
- Native MeshRenderer component using AssetGuid mesh/material references.
- Reflection metadata + Scene serialization codecs for render components.
- Editor generic property accessors for render components.
- RenderSnapshot extraction from active World objects.
- Hierarchy-resolved world matrices inside RenderSnapshot.
- Column-major Mat4 math.
- Affine inverse.
- Perspective and orthographic Vulkan depth-range projections.
- Camera world/view/projection/view-projection construction.
- CPU MeshData contract.
- Built-in unit Cube and Quad geometry with normals/UVs/bounds.

### Imported render assets

Implemented:
- AssetGuid -> validated cached texture metadata resolution.
- AssetGuid + import-fingerprint decoded texture cache.
- PNG/JPEG -> RGBA8 decoding through pinned vendored stb_image.
- BMP 24/32-bit uncompressed true-color -> normalized top-left RGBA8 decoding.
- TGA 24/32-bit uncompressed true-color -> normalized top-left RGBA8 decoding.
- Per-device AssetGuid Vulkan texture/material cache that uploads decoded RGBA8 pixels, generates a complete gamma-aware mip chain, and reuses matching fingerprints.
- First .nmat Material AssetGuid format containing a base-color Texture AssetGuid.
- Per-device Vulkan Material AssetGuid cache resolving .nmat -> texture cache -> combined-image-sampler descriptor.
- AssetGuid -> validated cached model metadata resolution.
- AssetGuid + import-fingerprint decoded MeshData cache for glTF/GLB/OBJ geometry.
- Per-device AssetGuid Vulkan mesh cache reusing uploaded vertex/index buffers while preserving submesh ranges.
- VK Preview can resolve imported glTF/GLB/OBJ MeshRenderer.mesh GUIDs through the project cache.
- MeshRenderer.material can resolve a .nmat AssetGuid to a real imported PNG/JPEG/BMP/TGA texture and Vulkan descriptor, with diagnostic material fallback on failure.
- When no explicit material is assigned, glTF/GLB primitive material slots prefer import-time cooked generated .nmat/Texture subassets; PNG/JPEG bufferView, data-URI, external image and baseColorFactor paths are baked to ordinary cached texture/material artifacts. Direct glTF decode remains a compatibility fallback.
- glTF PBR baseColorFactor: synthesizes a 1x1 sRGB texture for image-free color materials, or multiplies the image in linear space then encodes to sRGB; alpha channel is multiplied linearly.
- Explicit .nmat overrides the automatic glTF texture; a missing/unsupported auto texture falls back to diagnostic material without repeated parsing every frame.
- Imported mesh/material GPU caches are invalidated after asset filesystem changes.
- AssetGuid -> shader descriptor + SPIR-V word resolution.
- Descriptor/source consistency checks for shader word counts and SPIR-V magic.

### Vulkan runtime bootstrap

Implemented:
- Vulkan dynamic loader.
  - vulkan-1.dll on Windows.
  - libvulkan.so.1 on POSIX.
- Instance extension enumeration.
- VkInstance creation/destruction.
- Physical-device enumeration.
- Graphics queue-family selection.
- Optional presentation-surface support requirement.
- Logical VkDevice + graphics queue creation/destruction.
- Win32 VkSurfaceKHR creation/destruction.
- Swapchain format/present-mode/extent selection.
- VkSwapchainKHR creation and image enumeration.
- Resize/recreate lifecycle through VulkanContext.
- Swapchain image views.
- Color-only render pass.
- Shared device-local depth image + depth image view.
- Color+depth render pass.
- Per-swapchain-image framebuffers using the shared depth attachment.
- Command pool/buffers.
- Acquire/semaphore/fence/submit/present frame synchronization.
- Render-pass clear/present frame path.
- Graceful diagnostic fallback when Vulkan/ICD/surface support is unavailable.

### Vulkan GPU resources

Implemented:
- Vulkan buffer allocation/binding.
- Physical-device memory-type selection.
- Host-visible coherent buffer upload.
- Device-local buffer initialization through a synchronous staging buffer and vkCmdCopyBuffer.
- CPU MeshData -> device-local GPU vertex/index buffers.
- SPIR-V VkShaderModule creation/destruction contract.
- Reusable single-color VulkanRenderPass resource.
- Fixed MeshVertex graphics pipeline with:
  - position/normal/uv vertex layout.
  - dynamic viewport/scissor.
  - 64-byte MVP vertex push constant.
- Indexed mesh draw command recording with vkCmdDrawIndexed.
- Built-in diagnostic GLSL sources + audited SPIR-V fixtures.
- Headless CI graphics-pipeline creation using the diagnostic shaders.
- Opt-in Win32 Scene View Vulkan Preview.
- Stable built-in Unit Cube/Quad AssetGuids.
- Built-in CPU mesh GUID resolver and per-device Vulkan GPU mesh cache.
- Multi-draw indexed submission in one render pass.
- VK Preview renders the actual presentation World Camera + supported built-in MeshRenderer snapshot.
- Depth-tested pipelines with depth clear/write/LESS compare.

Implemented:
- RGBA8 Vulkan texture upload from decoded CPU pixels with full 1x1 mip-chain generation.
- Host-visible staging buffer -> device-local sampled VkImage.
- Image layout transitions to shader-read-only.
- Texture VkImageView.
- Default linear/repeat VkSampler.
- Textured material descriptor resource:
  - set 0 / binding 0 combined image sampler layout.
  - descriptor pool + descriptor set allocation.
  - image view/sampler descriptor update.
- Graphics pipeline layouts can optionally include the material descriptor-set layout.
- Indexed Vulkan mesh draws can carry an optional material resource and bind set 0 through vkCmdBindDescriptorSets.
- Diagnostic textured vertex/fragment GLSL sources with audited SPIR-V fixtures.
- Textured vertex path forwards MeshVertex UVs to the fragment stage.
- Diagnostic fragment shader samples set 0 / binding 0 sampler2D.
- VK Preview uses a diagnostic fallback material, imported .nmat/PBR material descriptors, per-submesh cooked model materials and direct glTF fallback when required.
- SpriteRenderer renders Texture AssetGuids as PPU-scaled XY quads through a depthless alpha-blended Vulkan pipeline with flip/sort support.
- SpriteAnimation clips are imported/cached and update SpriteRenderer frame textures during Play Mode.
- Headless Vulkan CI validates texture upload, sampler creation, material descriptors, textured shader modules, a real texture-sampling graphics pipeline, .nmat -> texture descriptors and glTF auto-texture upload. CPU tests cover GLB JSON/BIN geometry plus PNG bufferView, glTF PNG data URIs, baseColorFactor and sRGB tinting, staged external .gltf BIN/PNG rendering resources, sidecar watcher invalidation/recovery and unsafe URI rejection.

Not yet implemented:
- Full sampler-state cooking and remaining production PBR validation/shadow integration.
- Shader reflection and HLSL compiler-path parity; GLSL compilation/embed tooling exists.
- General descriptor/uniform binding beyond the first texture slot.
- Vulkan Game View.
- Production lighting/shadows and remaining full-PBR validation.
- Android Vulkan surface.

## RHI boundary

Implemented vocabulary/contracts:
- backend selection.
- opaque buffer/texture/pipeline handles.
- buffer/texture/pipeline descriptors.
- swapchain descriptor.
- RenderDevice interface.

The concrete Vulkan backend currently grows beneath this contract. The long-term public renderer should not expose raw Vulkan objects to gameplay/editor systems.

## Input foundation
- Cross-platform InputState tracks held/pressed/released key and mouse-button transitions per frame.
- Pointer state tracks position, delta and vertical wheel accumulation.
- Win32 platform event polling feeds keyboard, mouse buttons, focus-loss releases and cursor position into InputState.
- ActionMap can bind multiple keys/buttons to named actions and query held/pressed/released aggregation.
- EditorModel receives the platform InputState every frame.
- Managed ABI v12 has a separate native Input callback table; generated C# exposes Unity-familiar KeyCode, Input.GetKey/GetKeyDown/GetKeyUp, mousePosition, mouseDelta and mouseScrollDelta.
- Cross-platform tests cover same-frame press/release, focus loss, pointer accumulation, action bindings and real managed C# Input callbacks.

Not yet implemented:
- Gamepad and touch device backends.
- Persistent project Input Action asset/editor.
- Analog axes/composites/rebinding.

## Immediate next work

1. Continue the ABI v12 generic property bridge into physics/audio components as those runtime systems land.
2. Package/version the managed NEngine API surface and add debugger attach/symbol workflow.
3. Finish production PBR validation: sampler state, lights/shadows and remaining material behavior.
4. Decide and implement the explicit policy for remote/nonlocal or outside-directory glTF resources while preserving sandbox safety.
5. Add WebP decoding plus texture compression/transcoding policy; mipmap generation/upload is already implemented.
6. Choose a vendored FBX decoder strategy and add FBX mesh/material cooking.
7. Replace the GDI Scene View presentation only together with a Vulkan Editor Camera + matching picking/gizmo projection.
8. Extend shader tooling with reflection and validated HLSL compiler parity.

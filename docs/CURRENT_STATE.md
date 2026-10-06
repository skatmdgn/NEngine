# Current state

Version: 0.3.0-dev
Milestone: Asset database is integrated with the first real Vulkan runtime/resource path.

## Verified baseline

The user previously verified the 0.2.6 Windows editor runtime:
- Editor opens and remains stable.
- Hierarchy / selection / Inspector work.
- Name and Transform editing work.
- Undo / Redo work.
- Play / Pause / Step / Stop work.
- Scene Save / Open round-trip works.
- UTF-8 names including Korean display correctly.

Newer 0.3/0.4 work is continuously built and tested on Windows + Ubuntu CI. A fresh user-side Windows acceptance pass is still required after the Vulkan/editor changes.

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

The Scene View still uses GDI by default for interactive diagnostic object/gizmo drawing. An opt-in **VK Preview** toolbar toggle now renders the actual presentation World through Vulkan when supported: it reads the active Camera plus MeshRenderer items from RenderSnapshot, resolves built-in Cube/Quad AssetGuids and supported imported glTF/GLB AssetGuids to cached GPU meshes, computes per-object MVP matrices, and submits multiple indexed draws in one render pass. Imported geometry uses an explicitly assigned .nmat texture when available; if material is unset, supported glTF/GLB first-primitive PBR base-color PNG/JPEG or linear baseColorFactor is decoded/baked and sampled automatically. Otherwise the renderer retains its diagnostic textured fallback. Turning the toggle off immediately returns to the GDI interaction view.

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
- Import fingerprint/cache manifest.
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
- External .gltf sidecar BIN/image staging, AssetGuid dependency graph edges, size/timestamp import fingerprints, and watcher-triggered parent reimport; deleting/restoring a sidecar with persistent .meta triggers recovery.
- First glTF 2.0 geometry decode path:
  - .glb 2.0 JSON/BIN chunks.
  - .gltf base64 data-URI buffers.
  - external .gltf buffer/image sidecars copied into matching nested paths in the import cache, with sandboxed relative-URI checks.
  - TRIANGLES primitives.
  - float POSITION/NORMAL/TEXCOORD_0.
  - unsigned byte/short/int indices.
  - interleaved byteStride support.
  - multiple mesh primitives concatenated into MeshData.
  - right-handed glTF -> NEngine left-handed Z reflection + winding conversion.
  - glTF external buffer/image path traversal and unsupported URI forms rejected.
- SPIR-V shader import.
  - .spv validation by size/magic.
  - .vert.spv/.frag.spv stage hints.
  - shader.nasset descriptor.
- Automatic import feedback in Console.

Not yet implemented:
- WebP pixel decoding and production texture transcoding/mipmap/compression path.
- Full glTF multi-primitive/multi-material cooking into independent NEngine material/texture AssetGuids, advanced PBR factors/maps, node transforms, skins, morphs, sparse/quantized accessors.
- Sidecar change detection by content hash (currently size/write timestamp), restoration without a preserved .meta GUID, encoded/remote glTF URIs and sidecars outside the glTF source directory.
- OBJ/FBX mesh decoding/cooking.
- Shader source compilation (GLSL/HLSL -> SPIR-V).
- Audio decode/stream runtime.
- Dependency extraction from asset contents.

## C# / IDE foundation

Implemented:
- Managed project generator.
- .sln / .csproj generation.
- Assets/Scripts source inclusion.
- managed-packages.txt NuGet reference manifest.
- Visual Studio/Rider/default .sln association open path.

Not yet implemented:
- .NET host/runtime embedding.
- Gameplay assembly compile/load.
- hot reload.
- managed component discovery.
- debugger attach integration.

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
- Per-device AssetGuid Vulkan texture/material cache that uploads decoded RGBA8 pixels and reuses matching fingerprints.
- First .nmat Material AssetGuid format containing a base-color Texture AssetGuid.
- Per-device Vulkan Material AssetGuid cache resolving .nmat -> texture cache -> combined-image-sampler descriptor.
- AssetGuid -> validated cached model metadata resolution.
- AssetGuid + import-fingerprint decoded MeshData cache for glTF/GLB geometry.
- Per-device AssetGuid Vulkan mesh cache reusing uploaded vertex/index buffers.
- VK Preview can resolve imported glTF/GLB MeshRenderer.mesh GUIDs through the project cache.
- MeshRenderer.material can resolve a .nmat AssetGuid to a real imported PNG/JPEG/BMP/TGA texture and Vulkan descriptor, with diagnostic material fallback on failure.
- When no explicit material is assigned, GLB/glTF first-primitive PBR baseColorTexture automatically decodes a PNG/JPEG bufferView, base64 data URI or staged local external image into RGBA8 and uploads it as a sampled Vulkan material.
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
- RGBA8 Vulkan texture upload from decoded CPU pixels.
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
- VK Preview uses a 2x2 diagnostic RGBA8 texture/material as the fallback material; .nmat MeshRenderer.material binds explicitly imported texture descriptors and an unset material can sample the first glTF PBR base-color texture.
- Headless Vulkan CI validates texture upload, sampler creation, material descriptors, textured shader modules, a real texture-sampling graphics pipeline, .nmat -> texture descriptors and glTF auto-texture upload. CPU tests cover GLB JSON/BIN geometry plus PNG bufferView, glTF PNG data URIs, baseColorFactor and sRGB tinting, staged external .gltf BIN/PNG rendering resources, sidecar watcher invalidation/recovery and unsafe URI rejection.

Not yet implemented:
- Automatic glTF material/texture dependency cooking into NEngine material assets.
- Shader source compiler and reflection.
- General descriptor/uniform binding beyond the first texture slot.
- Vulkan Game View.
- PBR/lights/shadows/sprites.
- Android Vulkan surface.

## RHI boundary

Implemented vocabulary/contracts:
- backend selection.
- opaque buffer/texture/pipeline handles.
- buffer/texture/pipeline descriptors.
- swapchain descriptor.
- RenderDevice interface.

The concrete Vulkan backend currently grows beneath this contract. The long-term public renderer should not expose raw Vulkan objects to gameplay/editor systems.

## Immediate next work

1. Extend the first glTF PBR base-color preview path to proper multi-material/image extraction and persistent .nmat/Texture AssetGuid cooking.
2. Extend external glTF sidecar support to encoded URIs, outside-directory policies and stable recovery without .meta (basic same-directory staging/tracking completed).
3. Add WebP decoding plus mipmap/compression/transcoding policy behind the decoded-texture cache.
4. Add node-transform-aware glTF scene/mesh cooking plus quantized/sparse accessor support where needed.
5. Add a shader compiler toolchain path rather than making glslang/DXC a hidden build dependency.
6. Return to .NET hosting after the renderer/resource boundary is stable.

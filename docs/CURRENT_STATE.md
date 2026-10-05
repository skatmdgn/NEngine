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

The Scene View still uses GDI by default for interactive diagnostic object/gizmo drawing. An opt-in **VK Preview** toolbar toggle now renders the actual presentation World through Vulkan when supported: it reads the active Camera plus MeshRenderer items from RenderSnapshot, resolves built-in Cube/Quad AssetGuids to cached GPU meshes, computes per-object MVP matrices, and submits multiple indexed draws in one render pass. Turning the toggle off immediately returns to the GDI interaction view.

## Project / Asset database

Implemented:
- Persistent NEngine.nproject manifest.
- Safe project-relative startup Scene validation.
- Project bootstrap with Assets/Scenes, Assets/Scripts, ProjectSettings, Packages and Library/Cache.
- Persistent startup Main.nscene.
- Startup Scene contains native Camera and Light components.
- GUID + .meta asset identity.
- Extension importer registry.
- Polling file watcher.
- Automatic rescan and reimport for changed files.
- Import fingerprint/cache manifest.
- Validated cache artifact lookup rejecting stale source/importer versions.
- Dependency graph forward/reverse edges.
- Assets panel backed by AssetDatabase.
- Scene/script/raw source staging.
- Texture source staging + metadata descriptor.
  - PNG/BMP/TGA/JPEG dimension probing where supported.
- Audio source staging + WAV metadata descriptor.
  - channels, sample rate, bits/sample, data bytes.
- Model source staging + format/source-size descriptor.
- SPIR-V shader import.
  - .spv validation by size/magic.
  - .vert.spv/.frag.spv stage hints.
  - shader.nasset descriptor.
- Automatic import feedback in Console.

Not yet implemented:
- Pixel decoding/transcoding.
- Real mesh decoding/cooking (glTF/OBJ/FBX).
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
- AssetGuid -> validated cached model metadata resolution.
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
- Headless Vulkan CI validates texture upload, sampler creation, material descriptors and textured-compatible pipeline layout creation.

Not yet implemented:
- A fragment shader that actually samples the bound texture.
- Material AssetGuid/serialization model and imported material cache.
- Shader source compiler and reflection.
- General descriptor/uniform binding beyond the first texture slot.
- Imported mesh rendering and a Vulkan Game View.
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

1. Add a diagnostic textured fragment shader and verify sampled pixels through the VK Preview path.
2. Add a small decoded-pixel texture cache keyed by AssetGuid.
3. Decode/cook a first real model format (glTF) into MeshData and reuse the existing GPU mesh upload path.
4. Make VK Preview cover imported mesh/material assets before replacing the GDI interaction view.
5. Add a shader compiler toolchain path rather than making glslang/DXC a hidden build dependency.
6. Return to .NET hosting after the renderer/resource boundary is stable.

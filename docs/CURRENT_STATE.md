# Current state

Version: 0.3.0-dev
Milestone: Asset database complete enough for renderer integration; Vulkan renderer foundation started.

## Verified baseline

The user previously verified the 0.2.6 Windows editor runtime:
- Editor opens and remains stable.
- Hierarchy / selection / Inspector work.
- Name and Transform editing work.
- Undo / Redo work.
- Play / Pause / Step / Stop work.
- Scene Save / Open round-trip works.
- UTF-8 names including Korean display correctly.

Newer 0.3 features are covered by Windows + Ubuntu CI, but still need another user-side Windows acceptance pass.

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
- Structured Console model with severity, filtering model and duplicate collapse.
- Selection + command stack + Undo/Redo.
- Undo-aware Scene dirty/savepoint tracking.
- Dirty marker in editor title.
- Play Mode cloned Runtime World.
- Click selection in Scene View.
- X/Z translation gizmo drag committed as one command.
- Generic reflection-driven Inspector presentation.
- Generic property text editing for bool/int/uint/float/string/Vec3/Quaternion/EntityReference/AssetReference.
- Generic SetPropertyCommand path for arbitrary registered components.
- Scene Open / Save.
- Project startup Scene load.
- Asset double-click routing for Scene/script/external assets.
- C# solution generation/open workflow.

The Scene View still uses GDI as a diagnostic/editor surface. It is intentionally separate from the new render runtime contracts.

## Project / Asset database

Implemented:
- Persistent NEngine.nproject manifest.
- Manifest validation including safe project-relative startup Scene path.
- New-project bootstrap with Assets/Scenes, Assets/Scripts, ProjectSettings, Packages and Library/Cache.
- Persistent startup Main.nscene.
- Startup Scene contains native Camera and Light components.
- GUID + .meta asset identity.
- Extension importer registry.
- Polling file watcher.
- Automatic rescan and reimport for changed files.
- Import fingerprint/cache manifest.
- Validated cache artifact lookup that rejects stale source/importer versions.
- Dependency graph forward/reverse edges.
- Assets panel backed by AssetDatabase.
- Scene/script/raw source staging.
- Texture source staging + metadata descriptor.
  - PNG/BMP/TGA/JPEG dimension probing where supported.
- Audio source staging + WAV metadata descriptor.
  - channels, sample rate, bits/sample, data bytes.
- Model source staging + format/source-size descriptor.
- Automatic import feedback in Console.

Not yet implemented:
- Pixel decoding/transcoding and GPU texture upload.
- Real mesh decoding/cooking (glTF/OBJ/FBX).
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

## Renderer 0.4 foundation

Implemented:
- NEngineRender module.
- Native Camera component.
- Native Light component.
- Native MeshRenderer component using AssetGuid mesh/material references.
- Reflection metadata + Scene serialization codecs for render components.
- Editor generic property accessors for render components.
- RenderSnapshot extraction from active World objects.
- RHI vocabulary/contracts for:
  - backend selection
  - opaque buffer/texture/pipeline handles
  - buffer/texture/pipeline descriptors
  - swapchain descriptor
  - RenderDevice interface
- Render component and Scene round-trip tests.

Not yet implemented:
- Vulkan instance/device/queues.
- Windows Vulkan surface/swapchain.
- command buffers/synchronization.
- shader compilation/reflection.
- GPU buffer/texture resource cache.
- mesh/material binding.
- camera matrices.
- actual draw submission.
- lighting/PBR/shadows/sprites.
- Android Vulkan surface.

## Immediate next work

1. Keep Windows + Ubuntu CI green for the 0.3/0.4 boundary.
2. Add renderer-facing asset resolver from AssetGuid -> validated imported artifacts.
3. Add CPU render resource models for imported texture/model descriptors.
4. Implement Vulkan backend bootstrap and Windows surface/swapchain.
5. Replace diagnostic Scene presentation progressively without coupling Editor interaction math to Vulkan.
6. Return to .NET hosting only after the render/resource boundary is stable.

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
- [ ] WebP decode + mipmap/compression/transcoding
- [x] .nmat importer + texture dependency graph
- [x] glTF/GLB geometry decode into MeshData
- [x] AssetGuid decoded/GPU mesh caches
- [ ] glTF material/image dependency cooking
- [ ] External .gltf sidecar dependency staging
- [ ] OBJ/FBX mesh decoding/cooking
- [ ] GLSL/HLSL shader compile toolchain

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
- [ ] Automatic glTF material/image -> NEngine material cooking
- [ ] Scene View Vulkan presentation replacement
- [ ] Android Vulkan surface
- [ ] PBR/lights/shadows/sprites

## 0.5 — C# scripting + IDE
- [ ] .NET host
- [ ] NEngine managed API assemblies
- [x] generated sln/csproj foundation
- [x] NuGet package-reference manifest foundation
- [ ] compile/reload
- [ ] Visual Studio attach/debug symbols
- [ ] coroutine/timer scheduler

## 0.6 — Physics, input, audio
- [ ] 3D physics backend integration
- [ ] 2D physics backend integration
- [ ] keyboard/mouse/gamepad/touch actions
- [ ] audio source/listener/mixer basics

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

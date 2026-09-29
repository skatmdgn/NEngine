# Roadmap

Roadmap entries are implementation order, not promises tied to dates.

## 0.1 — Core foundation
- [x] Entity/World lifetime
- [x] Stable component type registry foundation
- [x] Transform hierarchy
- [x] Scene data model
- [x] Versioned serializer
- [x] Tests and CI
- [ ] General component storage/reflection properties
- [ ] Prefab data model and override patch representation
- [ ] World clone/snapshot for Play Mode

## 0.2 — Windows editor foundation
- Native window/application layer
- Dockable editor shell
- Hierarchy / Inspector / Scene / Assets / Console
- Selection, commands, Undo/Redo
- Play/Pause/Step using cloned World

## 0.3 — Asset database
- GUID/meta database
- File watcher
- Importer contract
- Dependency graph/cache
- Texture and glTF first importers

## 0.4 — Vulkan renderer
- RHI contract
- Windows Vulkan surface/swapchain
- Android Vulkan surface
- Mesh/material/texture
- Camera, PBR, lights, shadows, sprites

## 0.5 — C# scripting + IDE
- .NET host
- NEngine managed API assemblies
- generated sln/csproj
- compile/reload
- Visual Studio attach/debug symbols
- coroutine/timer scheduler

## 0.6 — Physics, input, audio
- 3D physics backend integration
- 2D physics backend integration
- keyboard/mouse/gamepad/touch actions
- audio source/listener/mixer basics

## 0.7 — Animation/UI/navigation
- animation clips/state machine/blend tree subset
- runtime Canvas-style UI
- NavMesh agent/obstacle/link subset
- particle system

## 0.8 — Packages/plugins
- NuGet
- managed DLL
- Windows native DLL
- Android AAR/JAR/SO/Maven contributions
- dependency resolution
- adapter SDK

## 0.9 — Build/toolchain/productization
- Windows Player exporter
- Android APK/AAB exporter
- side-by-side Android toolchain profiles
- updater/package cache
- profiler/frame debugger
- installer/signing hooks

## 1.0 — First complete product target
A Unity-familiar developer can create and ship ordinary 2D/general 3D Windows/Android games without learning a fundamentally new object/workflow model.

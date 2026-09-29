# NEngine architecture

## Product definition
NEngine aims to let a Unity-experienced developer build the same class of ordinary 2D/3D Windows/Android game with minimal retraining. It does not aim to load Unity projects, execute arbitrary Unity binaries, or clone every Unity package.

## Architectural ceiling
The engine stays understandable by keeping about twenty first-class subsystems with explicit one-way contracts:

1. Core / memory primitives
2. Object World
3. Serialization / Scene / Prefab
4. Assets / import database
5. Scripting / C# host / IDE integration
6. Renderer / RHI
7. Physics 3D
8. Physics 2D
9. Animation
10. Audio
11. Input
12. Runtime UI
13. Navigation
14. Jobs
15. Editor
16. Build
17. Platform
18. Profiler / diagnostics
19. Package Manager / Plugin Runtime
20. Toolchain Manager

## Dependency law
Higher layers may depend on lower contracts; lower layers never depend on editor or product-specific plugins.

Editor -> public engine API -> World -> runtime subsystems -> Assets/Jobs -> Platform

Examples of forbidden dependencies:
- Physics -> Editor
- Renderer -> Inspector
- Core -> AdMob/Firebase/Steam
- Scene serializer -> Visual Studio

## Object model
The user-facing object model is Unity-familiar, while internal storage is allowed to be data-oriented.

- `Entity` is a generational 64-bit handle.
- `World` owns entity lifetime.
- GameObject-style facades will resolve into World/component storage.
- Transform parenting rejects cycles and stale handles.

## Editor/runtime model
Editor and Player use the same World/component representation. Play Mode creates a runtime world from editor state and discards/restores runtime-only changes on Stop.

## Platform policy
- Editor: Windows x64 first.
- Player: Windows x64 + Android.
- Renderer: Vulkan-first to keep one graphics backend across both targets; a Windows D3D backend is optional later, not a 1.0 requirement.
- Android toolchains are versioned side-by-side profiles, not hardwired to an editor version.

## Scripting policy
C# is the gameplay language. Visual Studio is the primary external IDE, with Rider/VS Code integration later. Generated `.sln/.csproj` files are build artifacts, not authoritative project state.

## Plugin policy
External SDK-specific code stays outside engine core. First-class package contribution types are planned for:
- NuGet / managed assemblies
- Windows x64 native DLLs
- Android AAR / JAR / SO
- Maven dependencies
- manifest / Gradle / permission contributions

Unity-specific packages may require NEngine adapters. Arbitrary `.unitypackage` compatibility is not a goal.

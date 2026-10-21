# Build and acceptance

Development label: **0.9.0-dev**, pre-1.0; not a shippable Windows/Android game engine. See [CURRENT_STATE.md](CURRENT_STATE.md) for the current evidence and production limitations.

## Requirements

- CMake 3.25+ and a C++20 compiler.
- Real Windows 10/11 x64 for editor, Win32 Vulkan presentation, interactive player/audio acceptance.
- Vulkan-capable hardware/driver for Vulkan Preview and Player graphics.
- For managed gameplay: compatible installed .NET 8 SDK for building and hostfxr/runtime for execution; packaged managed files are not a self-contained .NET runtime.
- Android SDK/NDK/JDK/Gradle toolchain profiles and APK/AAB export are **not yet implemented**.

## Configure, build and test

```bash
cmake -S . -B build -DNENGINE_BUILD_TESTS=ON -DNENGINE_BUILD_EDITOR=ON -DNENGINE_BUILD_PLAYER=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

The GitHub Actions [NEngine CI](https://github.com/skatmdgn/NEngine/actions) workflow runs Ubuntu and Windows build/test jobs. At the 2026-10-16 UTC commit `51db15f`, both passed **11/11 CTest groups** and uploaded a Windows editor executable plus Player/exporter binaries. Passing tests and artifact upload are **not** interactive Windows GPU, C# deployment, sound-output or Android QA.

## Windows editor and Canvas authoring

`NEngineEditor.exe` has Hierarchy/Inspector/Scene/Assets/Console, property editing, Save/Open, Undo/Redo and Play/Pause/Step/Stop foundations. Scene editing normally uses GDI; **VK Preview** opt-in draws supported World meshes through Vulkan but is not a finished editor Game View.

In Canvas Edit Mode, double-click an imported `Assets/UI/*.ncanvas` asset. Select UI widgets by clicking the GDI Scene View or choosing the widget hierarchy dropdown. Authoring includes name/text changes, Add/Delete, Duplicate, To Back/Front, RGBA/anchor fields, preset anchors, parent reassignment, Ctrl+click multi-selection, drag move, Shift+drag resize and bounded Undo/Redo. **Save Canvas** writes/reimports the source after validation; unsaved work is guarded. Canvas editing is not equivalent to a production Vulkan runtime UI or general-purpose Unity UI authoring.

## Windows standalone Player and export

Build the executable + exporter, then use a project directory containing `NEngine.nproject` and an **unused** destination directory outside the project:

```powershell
cmake -S . -B build -DNENGINE_BUILD_PLAYER=ON
cmake --build build --config Release
.\build\apps\player\Release\NEnginePlayerBuild.exe "C:\Games\MyNEngineProject" "C:\Exports\MyGame"
```

Optional third argument: path to `NEnginePlayer.exe`. The builder creates `NEngine.player`, a packaged executable, startup Scene, allowlisted assets and cooked GUID-based artifact descriptors/sources. Sources and dependency paths are validated to prevent traversal/unsafe sidecars. Run the exported `NEnginePlayer.exe` on Windows with a suitable Vulkan device.

To include **prebuilt** managed gameplay, provide both explicit executable and managed-build directory as the third and fourth CLI arguments:

```powershell
.\build\apps\player\Release\NEnginePlayerBuild.exe "C:\Games\MyNEngineProject" "C:\Exports\MyGame" "C:\Build\NEnginePlayer.exe" "C:\MyManagedBuild"
```

The managed build directory must contain `GameScripts.dll`, `GameScripts.runtimeconfig.json`, `NEngine.Bridge.dll` and `NEngine.API.dll`; optional allowed files include `GameScripts.deps.json` and `GameScripts.pdb`. No C# source files or arbitrary plugin dependencies are automatically bundled. If managed files are present, Windows Player attempts hostfxr initialization; a missing/incompatible runtime is a visible startup failure. Without managed files, native runtime systems remain available but ScriptBehaviour game logic is unavailable.

## Current Player capabilities and limitations

- Native Player drives bounded fixed-step 2D/3D physics, WAV playback through available WASAPI output, and sprite animation/controller clocks. It can forward 2D/3D collision/trigger events to hosted scripts.
- GUID-driven cooked textures and model/material descriptor/source artifacts exist for supported .glb/.gltf/.obj and `.nmat`; supported relative model sidecars are collected. Missing/unsafe glTF sidecars abort atomic export.
- Focused Win32 keyboard/mouse input and selected managed Transform, Rigidbody2D and AudioSource properties plus 2D/3D ray/box/overlap queries are connected.
- **Still missing:** complete generated PBR material/texture subasset packaging for imported models, full managed component/action-map parity, production Vulkan UI fonts/text, general managed dependency/plugin packaging, Android runtime/builds, and hardware acceptance for real exported games.
- Packaged WAV audio is not a complete streaming compressed-audio solution; real WASAPI output needs physical testing.

For testable task boundaries and pending acceptance see [ROADMAP.md](ROADMAP.md), [CURRENT_STATE.md](CURRENT_STATE.md) and [DEVELOPMENT_WORKFLOW.md](DEVELOPMENT_WORKFLOW.md).

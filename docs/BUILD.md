# Build

## Requirements for core development
- CMake 3.25+
- C++20 compiler

## Configure/build/test
```bash
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

## Windows product builds
Windows CI is the authoritative early compiler check. Native editor/render/plugin QA still requires a real Windows 10/11 x64 machine; compiling successfully is not the same as runtime verification.

## Android
Android tooling is intentionally absent at the core milestone. It will be supplied by versioned Toolchain Manager profiles rather than relying on arbitrary system `JAVA_HOME`, NDK or Gradle installations.

## Windows Canvas authoring (0.7)

1. Open a project containing an imported `Assets/UI/*.ncanvas`. Double-click the Canvas in the **Assets** list to enter Edit Mode Canvas authoring; the **Canvas Edit** Inspector toggle can reopen the selected preview. An already-dirty draft cannot be replaced until saved.
2. With the GDI Scene View visible (VK Preview is disabled automatically upon entry), click a panel, button or text label to select its topmost visible, clipped widget. Name and visible text are separate fields in the Canvas section of Inspector.
3. Edit the **name** (optional unique ASCII identifier) or **text** and click **Apply Name + Text**. Use **+ Button**, **+ Text**, **Delete UI**, **UI Undo/Redo**, or Scene View shortcuts Ctrl+Z/Ctrl+Y. The editing draft never modifies the imported asset while unsaved.
4. Click **Save Canvas** (or Ctrl+S while Scene View is focused) to validate, write a same-directory temporary file, preserve a rollback backup, replace the source and trigger reimport. Saving is disabled during Play Mode. The UI's **Canvas Edit** toggle will refuse to close a draft with unsaved changes.

The native authoring Inspector is preliminary. Layout anchors, clipping, colors, duplicate/reparent/sibling order have engine APIs but do not yet have complete Windows authoring controls. Runtime Vulkan Game View, production fonts and drag-resize handles are not yet implemented.

### Canvas authoring gestures and fields

In Windows Canvas Edit Mode, use the widget dropdown at the top of Inspector to choose an entry (including offscreen/hidden widgets). A click in GDI Scene View also selects the topmost visible/clipped widget. **Drag** to move the selected widget using anchor offsets, or hold **Shift** when beginning the drag to adjust its width/height through the right/bottom offsets; changing the view while dragging cancels the gesture. Each completed drag is a single Undo step. The widget actions **Duplicate**, **To Back**, and **To Front** operate on complete subtrees. Numeric fields are ordered **anchor_min.x, anchor_min.y, anchor_max.x, anchor_max.y**, then **offset_min.x, offset_min.y, offset_max.x, offset_max.y**, with four **RGBA 0–1** fields; click **Apply Anchors + RGBA** to commit. An invalid numeric value is rejected rather than storing nonfinite geometry. Save Canvas to persist source changes.

### Canvas 0.7 — Anchor presets, multi-selection and hierarchy editing

While editing a Canvas, pick a widget in the hierarchy dropdown or click it in the GDI Scene View. Hold **Ctrl while clicking** to add/remove widgets from the multi-selection (an outline appears around each visible selected item). **Drag** a selected widget to move the selected group once; selected descendants inherit their selected parent's translation instead of receiving a second delta. **Shift+drag** resizes the primary selection only. Releasing commits one Undo snapshot, and capture loss cancels the preview.

Select one of 12 anchor presets (nine fixed compass alignments, Stretch Horizontal, Stretch Vertical, Stretch All) then click **Apply Preset**. The widget keeps its current pixel rectangle at that viewport size; future parent resizing follows its new anchors. To change hierarchy, choose a parent in the **Parent** dropdown and click **Reparent**. Cycles and invalid/unavailable parents are rejected and the widget retains its pixel rectangle when changing parent. Press **Delete UI** to delete all selected subtrees in one Undo step. Save Canvas to persist changes.

## Windows Player bootstrap (0.9 work in progress)

Configure and compile the standalone Player and export command:

```powershell
cmake -S . -B build -DNENGINE_BUILD_PLAYER=ON
cmake --build build --config Release
.\build\apps\player\Release\NEnginePlayerBuild.exe "C:\Games\MyNEngineProject" "C:\Exports\MyGame"
```

The first argument is a project directory containing `NEngine.nproject`; the second is a **new, nonexistent** destination directory outside the project. By default the builder finds `NEnginePlayer.exe` beside itself, or you can supply the compiled player executable as an optional third argument. The output contains `NEnginePlayer.exe`, `NEngine.player`, `Content/Main.nscene` and an allowlisted copy of project assets under `Content/Assets/`. Run `NEnginePlayer.exe` in the exported directory. For CI, the Windows artifact `NEnginePlayerBootstrap-windows-x64` contains both build tools, but no project-specific game data. The editor artifact remains separate.

**Current limitations:** Only the startup Scene and raw allowed assets are packaged. Shader/texture/model cooked caches and GUID runtime asset resolution are not yet wired to the standalone Player, so custom imported visuals can be absent. `ScriptBehaviour` components deserialize but C# gameplay does **not** execute yet. Physics fixed ticks, animation scheduling, production audio, runtime UI and user-facing build profiles are not complete. Windows GPU initialization and real-game launch require manual driver/device QA; CI builds and tests, but cannot prove a player will run on arbitrary hardware. This is not a finished Windows game exporter and does not produce Android packages.

## Player runtime simulation and asset limitations

The Windows Player now drives `PlayerRuntime::advance(delta_seconds)` from the steady clock. Internally this runs 60 Hz fixed 2D/3D physics with up to eight substeps per frame and limits an excessive frame stall rather than attempting unlimited catch-up. The same host delta advances native WAV AudioSource clocks, sprite animation frame sampling and sprite controller transitions. A read-only GUID catalog scans the exported `Content/Assets` .meta files (and never repairs missing metadata in the shipped bundle). The importer/source directory tree must not escape the bundle by symlink. Exported WAV clips have a 64 MiB compressed/source decode-size cap; raw `.nanim` and `.nanimctrl` sources have 4 MiB each. WASAPI is attempted on Windows; a missing audio output device logs a diagnostic without preventing visuals or physics.

For testing, the `NEnginePlayerTests` target includes an exported Camera, SpriteRenderer/SpriteAnimator controller, Rigidbody2D, AudioSource and ScriptBehaviour Scene. It verifies GUID-based loading, WAV clocks, animation transitions, fixed physics substeps, pause, invalid manifest recovery and atomic export. The CI `NEnginePlayerCLIIntegration` performs independent builder -> packaged Player smoke coverage (headless on Linux; Windows package files checked but hardware rendering not exercised).

**Not yet a shippable ordinary game:** Player C# gameplay/managed physics callbacks are not running; GPU texture and mesh assets need a cooked-artifact resolver; UI Canvas is not yet presented through the Vulkan Player; there is no full audio streaming or Android Player build. Manual Windows GPU/WASAPI acceptance remains separate from successful automated builds.

## Windows Player cooked textures and optional managed gameplay

Player export cooks recognized project textures (.png/.jpg/.jpeg/.bmp/.tga/.webp) into `Content/Cooked/<AssetGuid>/` using the existing native texture importer. A read-only Player asset resolver supplies cooked texture descriptors and staged sources to the Vulkan World renderer, including SpriteRenderer texture GUIDs. This does not yet guarantee arbitrary model/material import or a textured sprite on untested GPUs; imported model/material asset cooking is a separate milestone.

To include **prebuilt** .NET C# gameplay, use the optional fourth CLI argument (the third remains the Player executable path):

```powershell
NEnginePlayerBuild.exe "C:\\MyProject" "C:\\MyExports\\Game" "C:\\Build\\NEnginePlayer.exe" "C:\\MyManagedBuild"
```

The managed build folder must contain `GameScripts.dll`, `GameScripts.runtimeconfig.json`, `NEngine.Bridge.dll`, and `NEngine.API.dll`. The exporter copies only these plus optional `GameScripts.deps.json` and `GameScripts.pdb`; it never includes C# source files or arbitrary plugins. On Windows, if `Content/Managed/GameScripts.dll` is present the Player looks for a compatible installed .NET hostfxr and initializes NEngine's existing managed bridge, running ScriptBehaviour FixedUpdate, Update/LateUpdate and 2D/3D contact event callbacks. Startup fails visibly if packaged C# runtime files exist but cannot be initialized. Without a DLL, native physics/animation/WAV playback still work, while ScriptBehaviour execution is unavailable.

This is **not** a bundled .NET redistributable, NuGet dependency resolver, complete Vulkan model cooker, full runtime Canvas renderer, or Android APK/AAB pipeline. Use real Windows hardware and .NET 8 for manual Player acceptance; successful CI builds verify C++ integration and packaging tests, not live managed gameplay or GPU presentation.

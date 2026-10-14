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

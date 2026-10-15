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

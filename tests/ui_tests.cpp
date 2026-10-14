#include <cmath>
#include <cstdlib>
#include <iostream>

#include "nengine/ui/canvas.hpp"
#include "nengine/ui/pointer_router.hpp"
#include "nengine/ui/keyboard_router.hpp"
#include "nengine/ui/rasterizer.hpp"
#include "nengine/ui/canvas_serialization.hpp"
#include "nengine/ui/canvas_asset.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace {
int failures = 0;
void check(bool ok, const char* name) {
    if (!ok) { ++failures; std::cerr << "FAIL: " << name << '\n'; }
}
} // namespace

int main() {
    using namespace nengine::ui;
    UiCanvas canvas;
    const auto panel = canvas.add(WidgetKind::Panel);
    auto* panel_data = canvas.find(panel);
    panel_data->anchors.offset_min = {50.0f, 40.0f};
    panel_data->anchors.offset_max = {350.0f, 240.0f};
    panel_data->clip_children = true;
    const auto button = canvas.add(WidgetKind::Button, panel);
    auto* button_data = canvas.find(button);
    button_data->anchors.anchor_min = {0.5f, 0.5f};
    button_data->anchors.anchor_max = {0.5f, 0.5f};
    button_data->anchors.offset_min = {-60.0f, -15.0f};
    button_data->anchors.offset_max = {60.0f, 15.0f};
    button_data->text = "Start";
    check(canvas.add(WidgetKind::Button, 987654u) == invalid_widget,
          "invalid parent cannot create orphan widget");
    const auto layout = canvas.layout(800.0f, 600.0f);
    check(layout.items.size() == 2u &&
          std::abs(layout.items[1].rect.x - 140.0f) < 0.0001f &&
          std::abs(layout.items[1].rect.y - 125.0f) < 0.0001f &&
          layout.items[1].rect.width == 120.0f &&
          layout.items[1].interactable &&
          layout.items[1].text == "Start",
          "parent relative anchors position a centered clickable button");

    const auto overflow = canvas.add(WidgetKind::Button, panel);
    auto* overflow_data = canvas.find(overflow);
    overflow_data->anchors.offset_min = {280.0f, 170.0f};
    overflow_data->anchors.offset_max = {380.0f, 230.0f};
    const auto clipped = canvas.layout(800.0f, 600.0f);
    check(clipped.items.size() == 3u &&
          clipped.items[2].clipped_rect.width == 20.0f &&
          clipped.items[2].clipped_rect.height == 30.0f,
          "child clipping intersects panel and viewport bounds");
    canvas.find(panel)->visible = false;
    check(canvas.layout(800.0f, 600.0f).items.empty(),
          "invisible parent hides its entire hierarchy");
    canvas.find(panel)->visible = true;
    check(canvas.remove(panel) && !canvas.find(button) &&
          !canvas.find(overflow) && canvas.layout(800.0f, 600.0f).items.empty(),
          "destroying parent removes descendants without ID reuse");
    UiCanvas hierarchy;
    const auto root_a = hierarchy.add(WidgetKind::Panel);
    const auto root_b = hierarchy.add(WidgetKind::Panel);
    const auto child_a = hierarchy.add(WidgetKind::Button, root_a);
    const auto grandchild_a = hierarchy.add(WidgetKind::Text, child_a);
    check(!hierarchy.reparent(root_a, grandchild_a) &&
          !hierarchy.reparent(root_a, root_a) &&
          !hierarchy.reparent(child_a, 999999u),
          "Canvas reparent rejects cycles self-parenting and stale parents");
    check(hierarchy.reparent(child_a, root_b) &&
          hierarchy.find(child_a)->parent == root_b &&
          hierarchy.find(grandchild_a)->parent == child_a &&
          hierarchy.widgets()[2].id == child_a &&
          hierarchy.widgets()[3].id == grandchild_a,
          "Canvas reparent moves the entire subtree after the new parent");
    std::stringstream moved_source;
    UiCanvas moved_readback;
    check(write_canvas(moved_source, hierarchy) &&
          read_canvas(moved_source, moved_readback) &&
          moved_readback.find(grandchild_a)->parent == child_a,
          "reparented hierarchy remains valid and serializable");

    UiCanvas sorting;
    const auto sorting_parent = sorting.add(WidgetKind::Panel);
    const auto sibling_a = sorting.add(WidgetKind::Button, sorting_parent);
    const auto sibling_b = sorting.add(WidgetKind::Button, sorting_parent);
    const auto nested_a = sorting.add(WidgetKind::Text, sibling_a);
    const auto sibling_c = sorting.add(WidgetKind::Button, sorting_parent);
    check(!sorting.set_sibling_index(0, 0) &&
          !sorting.set_sibling_index(345678, 0),
          "sibling reorder rejects invalid IDs");
    check(sorting.set_sibling_index(sibling_a, 999) &&
          sorting.widgets()[1].id == sibling_b &&
          sorting.widgets()[2].id == sibling_c &&
          sorting.widgets()[3].id == sibling_a &&
          sorting.widgets()[4].id == nested_a,
          "sibling reorder moves the full subtree to the front of paint order");
    check(sorting.set_sibling_index(sibling_a, 0) &&
          sorting.widgets()[1].id == sibling_a &&
          sorting.widgets()[2].id == nested_a,
          "sibling reorder moves full subtree behind older siblings");
    std::stringstream sorting_source;
    UiCanvas sorting_readback;
    check(write_canvas(sorting_source, sorting) &&
          read_canvas(sorting_source, sorting_readback) &&
          sorting_readback.find(nested_a)->parent == sibling_a,
          "sibling reorder maintains a serializable parent-first Canvas");

    sorting.find(sibling_a)->text = "Original";
    sorting.find(nested_a)->text = "Nested";
    const auto duplicate_a = sorting.duplicate_subtree(sibling_a);
    WidgetId duplicate_child = invalid_widget;
    for (const auto& node : sorting.widgets())
        if (node.parent == duplicate_a)
            duplicate_child = node.id;
    check(duplicate_a != invalid_widget && duplicate_a != sibling_a &&
          sorting.find(duplicate_a)->parent == sorting_parent &&
          sorting.find(duplicate_a)->text == "Original" &&
          duplicate_child != invalid_widget &&
          duplicate_child != nested_a &&
          sorting.find(duplicate_child)->text == "Nested",
          "Canvas duplication creates fresh IDs and preserves subtree data");
    sorting.find(duplicate_a)->text = "Copy";
    check(sorting.find(sibling_a)->text == "Original" &&
          sorting.find(duplicate_a)->text == "Copy",
          "editing duplicated Canvas widgets never mutates source data");
    std::stringstream duplication_source;
    UiCanvas duplication_readback;
    check(write_canvas(duplication_source, sorting) &&
          read_canvas(duplication_source, duplication_readback) &&
          duplication_readback.find(duplicate_child)->parent == duplicate_a,
          "duplicated Canvas hierarchy serializes with stable fresh IDs");

    const auto next = canvas.add(WidgetKind::Text);
    check(next > overflow && canvas.layout(0.0f, 600.0f).items.empty(),
          "widget IDs advance and invalid viewport is safely rejected");
    // Topmost button wins; clipped regions cannot receive hover/click.
    UiCanvas interactive;
    const auto bottom = interactive.add(WidgetKind::Button);
    auto* bottom_data = interactive.find(bottom);
    bottom_data->anchors.offset_max = {200.0f, 100.0f};
    const auto top = interactive.add(WidgetKind::Button);
    auto* top_data = interactive.find(top);
    top_data->anchors.offset_min = {50.0f, 0.0f};
    top_data->anchors.offset_max = {150.0f, 100.0f};
    UiPointerRouter router;
    auto interactive_layout = interactive.layout(400.0f, 200.0f);
    const auto enter = router.update(interactive_layout, {80.0f, 40.0f}, false);
    const auto down = router.update(interactive_layout, {80.0f, 40.0f}, true);
    const auto up = router.update(interactive_layout, {80.0f, 40.0f}, false);
    check(enter.size() == 1u && enter[0].target == top &&
          enter[0].type == UiPointerEventType::Enter &&
          down.size() == 1u && down[0].type == UiPointerEventType::Down &&
          up.size() == 2u && up[0].type == UiPointerEventType::Up &&
          up[1].type == UiPointerEventType::Click &&
          up[1].target == top,
          "topmost button receives hover down up and click");

    router.update(interactive_layout, {80.0f, 40.0f}, true);
    const auto moved = router.update(interactive_layout, {300.0f, 120.0f}, true);
    const auto released_outside =
        router.update(interactive_layout, {300.0f, 120.0f}, false);
    check(moved.size() == 1u &&
          moved[0].type == UiPointerEventType::Exit &&
          released_outside.size() == 1u &&
          released_outside[0].type == UiPointerEventType::Up,
          "captured release outside button never emits Click");

    interactive.find(top)->interactable = false;
    interactive_layout = interactive.layout(400.0f, 200.0f);
    const auto through_overlay =
        router.update(interactive_layout, {80.0f, 40.0f}, false);
    check(through_overlay.size() == 1u &&
          through_overlay[0].target == bottom &&
          through_overlay[0].type == UiPointerEventType::Enter,
          "noninteractable top widget allows lower button interaction");
    router.update(interactive_layout, {80.0f, 40.0f}, true);
    interactive.remove(bottom);
    interactive_layout = interactive.layout(400.0f, 200.0f);
    const auto destroyed_capture =
        router.update(interactive_layout, {80.0f, 40.0f}, false);
    bool clicked_destroyed = false;
    for (const auto& event : destroyed_capture)
        clicked_destroyed |= event.type == UiPointerEventType::Click;
    check(!clicked_destroyed && router.captured() == invalid_widget,
          "removed captured button never receives a ghost click");

    router.reset();
    router.update(interactive.layout(400, 200), {120.0f, 30.0f}, true);
    router.cancel_capture();
    const auto interrupted = router.update(
        interactive.layout(400, 200), {120.0f, 30.0f}, true);
    const auto interrupted_release = router.update(
        interactive.layout(400, 200), {120.0f, 30.0f}, false);
    bool any_false_click = false;
    for (const auto& event : interrupted_release)
        any_false_click |= event.type == UiPointerEventType::Click;
    check(router.captured() == invalid_widget &&
          !any_false_click && interrupted.size() <= 1u,
          "lost native capture cannot produce a ghost click");

    // Native keyboard/mouse InputState is consumable by the UI router.
    nengine::input::InputState native_input;
    native_input.set_pointer_position(80.0f, 40.0f);
    native_input.set_key(nengine::input::Key::MouseLeft, true);
    router.reset();
    const auto input_down = router.update(
        interactive.layout(400.0f, 200.0f), native_input);
    check(input_down.empty(),
          "native input does not emit clicks when no button is available");

    UiCanvas paint;
    const auto layer = paint.add(WidgetKind::Panel);
    auto* layer_node = paint.find(layer);
    layer_node->color = {1.0f, 0.0f, 0.0f, 1.0f};
    layer_node->anchors.offset_max = {5.0f, 5.0f};
    layer_node->clip_children = true;
    const auto translucent = paint.add(WidgetKind::Image, layer);
    auto* translucent_node = paint.find(translucent);
    translucent_node->color = {0.0f, 0.0f, 1.0f, 0.5f};
    translucent_node->anchors.offset_min = {3.0f, 3.0f};
    translucent_node->anchors.offset_max = {8.0f, 8.0f};
    const auto text_widget = paint.add(WidgetKind::Text);
    (void)text_widget;
    const auto paint_commands = build_ui_draw_list(paint.layout(8, 8));
    const auto image = rasterize_ui(paint_commands, 8u, 8u);
    const auto rgba = [&](std::uint32_t x, std::uint32_t y, int channel) {
        return image.pixels[(y * image.width + x) * 4u +
                            static_cast<std::size_t>(channel)];
    };
    check(paint_commands.size() == 2u &&
          image.pixels.size() == 8u * 8u * 4u &&
          rgba(0, 0, 0) == 255u && rgba(0, 0, 3) == 255u &&
          rgba(4, 4, 0) >= 126u && rgba(4, 4, 2) >= 126u &&
          rgba(6, 6, 3) == 0u,
          "CPU UI renderer blends ordered quads and respects child clipping");
    check(rasterize_ui({}, 5000u, 1u).pixels.empty(),
          "rasterizer guards oversized render targets");

    std::stringstream canvas_file;
    check(write_canvas(canvas_file, paint),
          "Canvas tree writes versioned .ncanvas source");
    UiCanvas restored_canvas;
    check(read_canvas(canvas_file, restored_canvas) &&
          restored_canvas.find(layer) &&
          restored_canvas.find(translucent) &&
          restored_canvas.find(translucent)->parent == layer &&
          restored_canvas.find(translucent)->color.a == 0.5f &&
          restored_canvas.widgets().size() == paint.widgets().size(),
          "Canvas source roundtrip retains stable IDs hierarchy and color");
    const auto new_widget = restored_canvas.add(WidgetKind::Button);
    check(new_widget > text_widget,
          "restored Canvas allocates new IDs above previously serialized IDs");
    std::stringstream invalid_canvas(
        "NENGINE_CANVAS 1\nWIDGETS 1\n"
        "WIDGET 100 999 3 1 1 0 \"Lost\" 0 0 0 0 0 0 10 10 1 1 1 1\n"
        "END_CANVAS\n");
    const auto old_widgets = restored_canvas.widgets().size();
    check(!read_canvas(invalid_canvas, restored_canvas) &&
          restored_canvas.widgets().size() == old_widgets,
          "orphan Canvas widget is rejected without damaging existing UI");

    const auto root = std::filesystem::temp_directory_path() /
        ("nengine_canvas_import_" + std::to_string(
            std::chrono::high_resolution_clock::now()
                .time_since_epoch().count()));
    std::filesystem::create_directories(root / "cache");
    const auto source = root / "menu.ncanvas";
    {
        std::ofstream output(source, std::ios::binary);
        check(write_canvas(output, paint),
              "Canvas source writer generates importable file");
    }
    nengine::assets::AssetRecord record;
    record.guid = nengine::assets::AssetGuid::generate();
    record.source_path = source;
    record.relative_path = "menu.ncanvas";
    record.importer_id = "NEngine.Canvas";
    record.file_size = std::filesystem::file_size(source);
    nengine::assets::ImporterDescriptor descriptor{
        "NEngine.Canvas", 1u, {".ncanvas"}, false};
    const nengine::assets::ImportContext context{
        &record, &descriptor, root / "cache"};
    const auto result = canvas_source_importer(context);
    check(result.success && result.artifacts.size() == 1u,
          "Canvas importer stages validated source");
    nengine::assets::CachedArtifactSet artifacts;
    artifacts.fingerprint = "canvas-v1";
    artifacts.artifacts = result.artifacts;
    CanvasAssetCache canvas_cache;
    const auto* cached = canvas_cache.load(record.guid, artifacts);
    check(cached && cached->widgets().size() == paint.widgets().size() &&
          canvas_cache.find(record.guid) == cached,
          "fingerprint-aware Canvas cache resolves staged UI");
    artifacts.fingerprint = "canvas-v2";
    artifacts.artifacts.clear();
    check(!canvas_cache.load(record.guid, artifacts) &&
          !canvas_cache.find(record.guid),
          "invalidated Canvas cache does not return stale UI");
    std::error_code ec;
    std::filesystem::remove_all(root, ec);

    UiCanvas keyboard_canvas;
    const auto keyboard_a = keyboard_canvas.add(WidgetKind::Button);
    const auto keyboard_b = keyboard_canvas.add(WidgetKind::Button);
    keyboard_canvas.find(keyboard_b)->anchors.offset_min = {150, 0};
    keyboard_canvas.find(keyboard_b)->anchors.offset_max = {250, 30};
    UiKeyboardRouter keyboard;
    const auto keys = keyboard_canvas.layout(400, 100);
    auto key_events = keyboard.update(keys, true, false);
    check(key_events.size() == 1u &&
          key_events[0].target == keyboard_a &&
          keyboard.focused() == keyboard_a,
          "Tab selects first interactable UI button");
    check(keyboard.update(keys, true, false).empty(),
          "held Tab does not repeat focus navigation");
    keyboard.update(keys, false, false);
    keyboard.update(keys, true, false);
    check(keyboard.focused() == keyboard_b,
          "second Tab moves focus to next button");
    keyboard.update(keys, false, false);
    keyboard.update(keys, true, false, true);
    check(keyboard.focused() == keyboard_a,
          "Shift+Tab traverses UI focus backwards");
    const auto key_down = keyboard.update(keys, false, true);
    check(key_down.size() == 1u &&
          key_down[0].target == keyboard_a &&
          key_down[0].type == UiPointerEventType::Down,
          "Enter press raises a button Down event");
    check(keyboard.update(keys, false, true).empty(),
          "held Enter cannot auto-repeat a click");
    const auto activate = keyboard.update(keys, false, false);
    check(activate.size() == 2u &&
          activate[0].target == keyboard_a &&
          activate[0].type == UiPointerEventType::Up &&
          activate[1].target == keyboard_a &&
          activate[1].type == UiPointerEventType::Click,
          "Enter release raises button Up then Click");
    keyboard.update(keys, false, false);
    const auto space_down = keyboard.update(keys, false, false, false, true);
    const auto space_up = keyboard.update(keys, false, false, false, false);
    check(space_down.size() == 1u &&
          space_down[0].type == UiPointerEventType::Down &&
          space_down[0].target == keyboard_a &&
          space_up.size() == 2u &&
          space_up[0].type == UiPointerEventType::Up &&
          space_up[1].type == UiPointerEventType::Click,
          "Space activates focused button like Enter");
    const auto cancel_down = keyboard.update(
        keys, false, true);
    const auto escaped = keyboard.update(
        keys, false, true, false, false, true);
    const auto cancelled_release = keyboard.update(
        keys, false, false);
    check(cancel_down.size() == 1u &&
          cancel_down[0].type == UiPointerEventType::Down &&
          escaped.size() == 1u &&
          escaped[0].type == UiPointerEventType::Exit &&
          keyboard.focused() == invalid_widget &&
          cancelled_release.empty(),
          "Escape clears keyboard focus and cancels an in-flight activation");
    keyboard.set_focus(keys, keyboard_b);
    check(keyboard.focused() == keyboard_b,
          "mouse-selected visible button becomes keyboard focused");
    keyboard.set_focus(keys, 1234567u);
    check(keyboard.focused() == keyboard_b,
          "invalid mouse target cannot steal keyboard focus");
    keyboard_canvas.find(keyboard_a)->visible = false;
    keyboard.update(keyboard_canvas.layout(400, 100), false, false);
    check(keyboard.focused() == invalid_widget,
          "hidden focused button loses keyboard focus");

    if (failures) return EXIT_FAILURE;
    std::cout << "All UI checks passed\n";
    return EXIT_SUCCESS;
}

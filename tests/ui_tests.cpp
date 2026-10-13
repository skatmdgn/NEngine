#include <cmath>
#include <cstdlib>
#include <iostream>

#include "nengine/ui/canvas.hpp"
#include "nengine/ui/pointer_router.hpp"
#include "nengine/ui/rasterizer.hpp"

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

    if (failures) return EXIT_FAILURE;
    std::cout << "All UI checks passed\n";
    return EXIT_SUCCESS;
}

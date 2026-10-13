#include <cmath>
#include <cstdlib>
#include <iostream>

#include "nengine/ui/canvas.hpp"

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
    if (failures) return EXIT_FAILURE;
    std::cout << "All UI checks passed\n";
    return EXIT_SUCCESS;
}

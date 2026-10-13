#include "nengine/ui/pointer_router.hpp"

namespace nengine::ui {

std::vector<UiPointerEvent> UiPointerRouter::update(
    const UiLayout& layout, core::Vec2 pointer, bool left_held) {

    std::vector<UiPointerEvent> events;
    WidgetId next_hover = invalid_widget;
    for (auto it = layout.items.rbegin(); it != layout.items.rend(); ++it) {
        if (it->interactable && it->kind == WidgetKind::Button &&
            it->clipped_rect.contains(pointer)) {
            next_hover = it->id;
            break;
        }
    }

    if (next_hover != hovered_) {
        if (hovered_ != invalid_widget)
            events.push_back({hovered_, UiPointerEventType::Exit});
        if (next_hover != invalid_widget)
            events.push_back({next_hover, UiPointerEventType::Enter});
        hovered_ = next_hover;
    }

    if (left_held && !was_held_ && hovered_ != invalid_widget) {
        captured_ = hovered_;
        events.push_back({captured_, UiPointerEventType::Down});
    } else if (!left_held && was_held_ &&
               captured_ != invalid_widget) {
        // A widget removed or hidden after Down cannot receive Click.
        bool still_visible = false;
        for (const auto& item : layout.items) {
            if (item.id == captured_ && item.interactable &&
                item.kind == WidgetKind::Button) {
                still_visible = true;
                break;
            }
        }
        if (still_visible) {
            events.push_back({captured_, UiPointerEventType::Up});
            if (captured_ == hovered_)
                events.push_back({captured_, UiPointerEventType::Click});
        }
        captured_ = invalid_widget;
    }
    was_held_ = left_held;
    return events;
}

std::vector<UiPointerEvent> UiPointerRouter::update(
    const UiLayout& layout, const input::InputState& input) {

    return update(
        layout,
        {input.pointer().x, input.pointer().y},
        input.held(input::Key::MouseLeft));
}

void UiPointerRouter::cancel_capture() noexcept {
    captured_ = invalid_widget;
    hovered_ = invalid_widget;
    // Suppress re-capture until a released-button frame is observed.
    was_held_ = true;
}

void UiPointerRouter::reset() noexcept {
    hovered_ = invalid_widget;
    captured_ = invalid_widget;
    was_held_ = false;
}

} // namespace nengine::ui

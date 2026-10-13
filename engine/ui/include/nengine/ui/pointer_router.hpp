#pragma once

#include <vector>

#include "nengine/input/input_state.hpp"
#include "nengine/ui/canvas.hpp"

namespace nengine::ui {

enum class UiPointerEventType {
    Enter, Exit, Down, Up, Click
};

struct UiPointerEvent {
    WidgetId target{invalid_widget};
    UiPointerEventType type{UiPointerEventType::Enter};
};

// Consumes UI layout + pointer state in screen pixel coordinates. The
// topmost interactable Button receives hover; Down captures that button,
// and Click fires only when released inside the same visible button.
class UiPointerRouter {
public:
    std::vector<UiPointerEvent> update(
        const UiLayout& layout, core::Vec2 pointer, bool left_held);
    std::vector<UiPointerEvent> update(
        const UiLayout& layout, const input::InputState& input);
    void reset() noexcept;
    WidgetId hovered() const noexcept { return hovered_; }
    WidgetId captured() const noexcept { return captured_; }

private:
    WidgetId hovered_{invalid_widget};
    WidgetId captured_{invalid_widget};
    bool was_held_{false};
};

} // namespace nengine::ui

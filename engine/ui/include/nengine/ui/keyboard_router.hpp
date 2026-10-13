#pragma once
#include <vector>
#include "nengine/ui/pointer_router.hpp"

namespace nengine::ui {

// Keyboard focus is independent of hover and mouse capture.
// Tab and Shift+Tab cycle interactable visible buttons.
// Enter activates on release, never on auto-repeat.
class UiKeyboardRouter {
public:
    std::vector<UiPointerEvent> update(
        const UiLayout& layout, bool tab_held, bool enter_held,
        bool reverse_tab = false);
    void reset() noexcept;
    WidgetId focused() const noexcept { return focused_; }

private:
    WidgetId focused_{invalid_widget};
    WidgetId enter_target_{invalid_widget};
    bool was_tab_held_{false};
    bool was_enter_held_{false};
};

} // namespace nengine::ui

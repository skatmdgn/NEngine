#include "nengine/ui/keyboard_router.hpp"
#include <algorithm>

namespace nengine::ui {
namespace {
bool focusable(const UiLayoutItem& item) noexcept {
    return item.kind == WidgetKind::Button && item.interactable &&
        item.clipped_rect.width > 0.0f &&
        item.clipped_rect.height > 0.0f;
}
} // namespace

std::vector<UiPointerEvent> UiKeyboardRouter::update(
    const UiLayout& layout, bool tab_held, bool enter_held, bool reverse_tab) {

    std::vector<UiPointerEvent> events;
    std::vector<WidgetId> buttons;
    for (const auto& item : layout.items)
        if (focusable(item)) buttons.push_back(item.id);

    if (std::find(buttons.begin(), buttons.end(), focused_) ==
        buttons.end()) {
        if (focused_ != invalid_widget)
            events.push_back({focused_, UiPointerEventType::Exit});
        focused_ = invalid_widget;
        enter_target_ = invalid_widget;
    }
    if (tab_held && !was_tab_held_ && !buttons.empty()) {
        const auto it = std::find(buttons.begin(), buttons.end(), focused_);
        const auto old_index = it == buttons.end()
            ? buttons.size()
            : static_cast<std::size_t>(it - buttons.begin());
        const auto next_index = old_index == buttons.size()
            ? (reverse_tab ? buttons.size() - 1u : 0u)
            : (reverse_tab
                ? (old_index + buttons.size() - 1u) % buttons.size()
                : (old_index + 1u) % buttons.size());
        if (focused_ != buttons[next_index]) {
            if (focused_ != invalid_widget)
                events.push_back({focused_, UiPointerEventType::Exit});
            focused_ = buttons[next_index];
            events.push_back({focused_, UiPointerEventType::Enter});
        }
        enter_target_ = invalid_widget;
    }
    if (enter_held && !was_enter_held_ &&
        focused_ != invalid_widget)
        enter_target_ = focused_;
    if (!enter_held && was_enter_held_ &&
        enter_target_ == focused_ &&
        enter_target_ != invalid_widget)
        events.push_back({focused_, UiPointerEventType::Click});
    if (!enter_held) enter_target_ = invalid_widget;
    was_tab_held_ = tab_held;
    was_enter_held_ = enter_held;
    return events;
}

void UiKeyboardRouter::reset() noexcept {
    focused_ = invalid_widget;
    enter_target_ = invalid_widget;
    was_tab_held_ = false;
    was_enter_held_ = false;
}

} // namespace nengine::ui

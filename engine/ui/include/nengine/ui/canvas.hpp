#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "nengine/core/math.hpp"

namespace nengine::ui {

using WidgetId = std::uint64_t;
inline constexpr WidgetId invalid_widget = 0u;

// Screen coordinates have top-left origin; all dimensions are pixels.
struct UiRect {
    float x{0.0f};
    float y{0.0f};
    float width{0.0f};
    float height{0.0f};

    bool contains(core::Vec2 point) const noexcept;
};

UiRect intersect_rect(UiRect a, UiRect b) noexcept;

struct UiColor {
    float r{1.0f};
    float g{1.0f};
    float b{1.0f};
    float a{1.0f};
};

struct RectAnchors {
    core::Vec2 anchor_min{};
    core::Vec2 anchor_max{};
    core::Vec2 offset_min{};
    core::Vec2 offset_max{100.0f, 30.0f};
};

enum class WidgetKind : std::uint8_t {
    Panel, Text, Image, Button
};

struct Widget {
    WidgetId id{invalid_widget};
    WidgetId parent{invalid_widget};
    WidgetKind kind{WidgetKind::Panel};
    RectAnchors anchors{};
    std::string text{};
    bool visible{true};
    bool interactable{true};
    bool clip_children{false};
    UiColor color{};
};

struct UiLayoutItem {
    WidgetId id{invalid_widget};
    WidgetKind kind{WidgetKind::Panel};
    UiRect rect{};
    UiRect clipped_rect{};
    std::string text{};
    bool interactable{false};
    UiColor color{};
};

struct UiLayout {
    std::vector<UiLayoutItem> items{};
};

class UiCanvas {
public:
    WidgetId add(WidgetKind kind, WidgetId parent = invalid_widget);
    // For persistence, restore an exact stable WidgetId (parent must exist).
    bool restore(Widget widget);
    const std::vector<Widget>& widgets() const noexcept { return widgets_; }
    Widget* find(WidgetId id) noexcept;
    const Widget* find(WidgetId id) const noexcept;
    bool remove(WidgetId id);
    void clear() noexcept;
    UiLayout layout(float width, float height) const;

private:
    std::vector<Widget> widgets_{};
    WidgetId next_id_{1u};
};

} // namespace nengine::ui

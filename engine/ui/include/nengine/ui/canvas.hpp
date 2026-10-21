#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "nengine/core/math.hpp"
#include "nengine/assets/asset_guid.hpp"

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
// Empty is allowed for legacy widgets; nonempty names are bounded ASCII IDs.
bool valid_widget_name(std::string_view name) noexcept;

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

// Editor drag math operates on a gesture's immutable starting anchors.
// resize=false translates both offsets; resize=true changes only far edge.
std::optional<RectAnchors> drag_canvas_anchors(
    RectAnchors start, core::Vec2 delta, bool resize) noexcept;

// Presets preserve the widget's absolute rectangle at the current
// authoring viewport, changing only its behavior under resizing.
enum class AnchorPreset : std::uint8_t {
    TopLeft, TopCenter, TopRight,
    MiddleLeft, MiddleCenter, MiddleRight,
    BottomLeft, BottomCenter, BottomRight,
    StretchHorizontal, StretchVertical, StretchAll
};
std::optional<RectAnchors> apply_anchor_preset(
    UiRect parent, UiRect rect, AnchorPreset preset) noexcept;

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
    // Stable optional script name, independent of visible widget text.
    std::string name{};
    // Image-only optional texture identity; zero is a solid-color fallback.
    assets::AssetGuid image_texture{};
};

struct UiLayoutItem {
    WidgetId id{invalid_widget};
    WidgetKind kind{WidgetKind::Panel};
    UiRect rect{};
    UiRect clipped_rect{};
    std::string text{};
    bool interactable{false};
    UiColor color{};
    assets::AssetGuid image_texture{};
};

struct UiLayout {
    std::vector<UiLayoutItem> items{};
};

// Topmost clipped widget in painter order. All-visible mode is suitable
// for authoring selection; button-only mode matches Play Mode routing.
WidgetId hit_test_ui(const UiLayout& layout, core::Vec2 pointer,
                     bool interactive_buttons_only = false) noexcept;

class UiCanvas {
public:
    WidgetId add(WidgetKind kind, WidgetId parent = invalid_widget);
    // For persistence, restore an exact stable WidgetId (parent must exist).
    bool restore(Widget widget);
    const std::vector<Widget>& widgets() const noexcept { return widgets_; }
    Widget* find(WidgetId id) noexcept;
    const Widget* find(WidgetId id) const noexcept;
    // Runtime authoring changes never alter persisted Canvas assets.
    bool set_text(WidgetId id, std::string text);
    bool set_name(WidgetId id, std::string name);
    WidgetId find_by_name(std::string_view name) const noexcept;
    bool set_visible(WidgetId id, bool visible) noexcept;
    bool set_interactable(WidgetId id, bool interactable) noexcept;
    // Reject non-finite, out-of-gamut RGBA values atomically.
    bool set_color(WidgetId id, UiColor color) noexcept;
    bool set_image_texture(WidgetId id, assets::AssetGuid guid) noexcept;
    bool set_anchors(WidgetId id, RectAnchors anchors) noexcept;
    WidgetId find_by_text(std::string_view text) const noexcept;
    bool remove(WidgetId id);
    // Move a complete widget subtree while preserving stable IDs.
    // Rejects cycles, stale IDs, and self-parenting.
    bool reparent(WidgetId id, WidgetId new_parent);
    // Reorder a widget (including descendants) among immediate siblings.
    // Larger sibling indices render on top. Indices are clamped.
    bool set_sibling_index(WidgetId id, std::size_t index);
    // Duplicate a complete subtree with fresh IDs and copied widget
    // properties; the duplicate root becomes a sibling of the source.
    WidgetId duplicate_subtree(WidgetId id);
    void clear() noexcept;
    UiLayout layout(float width, float height) const;

private:
    std::vector<Widget> widgets_{};
    WidgetId next_id_{1u};
};

} // namespace nengine::ui

#pragma once

#include <cstdint>
#include <vector>

#include "nengine/ui/canvas.hpp"

namespace nengine::ui {

struct UiDrawQuad {
    WidgetId widget{invalid_widget};
    UiRect rect{};
    UiColor color{};
};

// The draw list is in painter's order and its rectangles are clipped to
// the ancestor/viewport scissor. It is backend-independent.
std::vector<UiDrawQuad> build_ui_draw_list(const UiLayout& layout);

struct UiRgbaImage {
    std::uint32_t width{0};
    std::uint32_t height{0};
    // Straight-alpha RGBA8 row-major pixels.
    std::vector<std::uint8_t> pixels{};
};

// Deterministic CPU reference path for testing/debugging UI geometry.
// Text glyphs and external image textures are intentionally not implemented.
UiRgbaImage rasterize_ui(
    const std::vector<UiDrawQuad>& commands,
    std::uint32_t width, std::uint32_t height);

} // namespace nengine::ui

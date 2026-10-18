#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "nengine/ui/canvas.hpp"

namespace nengine::ui {

struct UiDrawQuad {
    WidgetId widget{invalid_widget};
    UiRect rect{};
    UiColor color{};
    std::string text{};
    bool paint_background{true};
    assets::AssetGuid image_texture{};
    // Unclipped rect used to retain correct texture UV after child clipping.
    UiRect source_rect{};
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

using UiImageResolver = std::function<const UiRgbaImage*(assets::AssetGuid)>;

// Deterministic CPU Canvas compositor. Missing image GUIDs retain solid-color
// fallback; loaded textures use nearest sampling, clipping, tint and alpha.
// The caller owns referenced images until rasterize_ui returns.
UiRgbaImage rasterize_ui(
    const std::vector<UiDrawQuad>& commands,
    std::uint32_t width, std::uint32_t height,
    const UiImageResolver& images = {});

} // namespace nengine::ui

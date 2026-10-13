#include "nengine/ui/rasterizer.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace nengine::ui {
namespace {
float channel(float value) noexcept {
    return std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : 0.0f;
}
std::uint8_t byte(float value) noexcept {
    return static_cast<std::uint8_t>(
        std::lround(channel(value) * 255.0f));
}
} // namespace

std::vector<UiDrawQuad> build_ui_draw_list(const UiLayout& layout) {
    std::vector<UiDrawQuad> commands;
    commands.reserve(layout.items.size());
    for (const auto& item : layout.items) {
        if (item.kind == WidgetKind::Text ||
            item.clipped_rect.width <= 0.0f ||
            item.clipped_rect.height <= 0.0f ||
            channel(item.color.a) == 0.0f)
            continue;
        commands.push_back({item.id, item.clipped_rect, item.color});
    }
    return commands;
}

UiRgbaImage rasterize_ui(
    const std::vector<UiDrawQuad>& commands,
    std::uint32_t width, std::uint32_t height) {

    UiRgbaImage image;
    constexpr std::uint32_t max_dimension = 4096;
    if (width == 0 || height == 0 ||
        width > max_dimension || height > max_dimension) return image;
    image.width = width;
    image.height = height;
    image.pixels.assign(static_cast<std::size_t>(width) * height * 4u, 0u);

    for (const auto& draw : commands) {
        if (!std::isfinite(draw.rect.x) ||
            !std::isfinite(draw.rect.y) ||
            !std::isfinite(draw.rect.width) ||
            !std::isfinite(draw.rect.height) ||
            draw.rect.width <= 0.0f || draw.rect.height <= 0.0f)
            continue;
        const auto clamp_bound = [](double value, std::uint32_t max) {
            return static_cast<std::uint32_t>(
                std::clamp(std::ceil(value), 0.0, static_cast<double>(max)));
        };
        const auto x0 = clamp_bound(draw.rect.x, width);
        const auto y0 = clamp_bound(draw.rect.y, height);
        const auto x1 = clamp_bound(
            static_cast<double>(draw.rect.x) + draw.rect.width, width);
        const auto y1 = clamp_bound(
            static_cast<double>(draw.rect.y) + draw.rect.height, height);
        if (x0 >= x1 || y0 >= y1) continue;

        const float src_a = channel(draw.color.a);
        const float src_r = channel(draw.color.r);
        const float src_g = channel(draw.color.g);
        const float src_b = channel(draw.color.b);
        if (src_a == 0.0f) continue;
        for (auto y = y0; y < y1; ++y) {
            for (auto x = x0; x < x1; ++x) {
                auto* pixel = image.pixels.data() +
                    (static_cast<std::size_t>(y) * width + x) * 4u;
                const float dst_a = pixel[3] / 255.0f;
                const float out_a = src_a + dst_a * (1.0f - src_a);
                const auto blend = [&](float src, unsigned channel_index) {
                    const float dst = pixel[channel_index] / 255.0f;
                    return out_a <= 0.0f ? 0.0f :
                        (src * src_a + dst * dst_a * (1.0f - src_a)) / out_a;
                };
                pixel[0] = byte(blend(src_r, 0));
                pixel[1] = byte(blend(src_g, 1));
                pixel[2] = byte(blend(src_b, 2));
                pixel[3] = byte(out_a);
            }
        }
    }
    return image;
}

} // namespace nengine::ui

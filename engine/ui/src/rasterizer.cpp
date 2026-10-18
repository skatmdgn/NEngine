#include "nengine/ui/rasterizer.hpp"

#include <algorithm>
#include <array>
#include <cctype>
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
// Small built-in 5x7 fallback glyphs. Supports the ASCII set without
// allocating OS font handles in the headless Vulkan/CTest build.
std::array<std::uint8_t, 7> glyph(unsigned char raw) noexcept {
    const char c = static_cast<char>(std::toupper(raw));
    switch (c) {
        case 'A': return {14,17,17,31,17,17,17};
        case 'B': return {30,17,17,30,17,17,30};
        case 'C': return {14,17,16,16,16,17,14};
        case 'D': return {30,17,17,17,17,17,30};
        case 'E': return {31,16,16,30,16,16,31};
        case 'F': return {31,16,16,30,16,16,16};
        case 'G': return {14,17,16,23,17,17,14};
        case 'H': return {17,17,17,31,17,17,17};
        case 'I': return {14,4,4,4,4,4,14};
        case 'J': return {7,2,2,2,18,18,12};
        case 'K': return {17,18,20,24,20,18,17};
        case 'L': return {16,16,16,16,16,16,31};
        case 'M': return {17,27,21,21,17,17,17};
        case 'N': return {17,25,21,19,17,17,17};
        case 'O': return {14,17,17,17,17,17,14};
        case 'P': return {30,17,17,30,16,16,16};
        case 'Q': return {14,17,17,17,21,18,13};
        case 'R': return {30,17,17,30,20,18,17};
        case 'S': return {15,16,16,14,1,1,30};
        case 'T': return {31,4,4,4,4,4,4};
        case 'U': return {17,17,17,17,17,17,14};
        case 'V': return {17,17,17,17,17,10,4};
        case 'W': return {17,17,17,21,21,21,10};
        case 'X': return {17,17,10,4,10,17,17};
        case 'Y': return {17,17,10,4,4,4,4};
        case 'Z': return {31,1,2,4,8,16,31};
        case '0': return {14,17,19,21,25,17,14};
        case '1': return {4,12,4,4,4,4,14};
        case '2': return {14,17,1,2,4,8,31};
        case '3': return {30,1,1,14,1,1,30};
        case '4': return {2,6,10,18,31,2,2};
        case '5': return {31,16,16,30,1,1,30};
        case '6': return {14,16,16,30,17,17,14};
        case '7': return {31,1,2,4,8,8,8};
        case '8': return {14,17,17,14,17,17,14};
        case '9': return {14,17,17,15,1,1,14};
        case ' ': return {};
        case '.': return {0,0,0,0,0,12,12};
        case ',': return {0,0,0,0,12,12,8};
        case '!': return {4,4,4,4,4,0,4};
        case '?': return {14,17,1,2,4,0,4};
        case ':': return {0,12,12,0,12,12,0};
        case '-': return {0,0,0,31,0,0,0};
        case '_': return {0,0,0,0,0,0,31};
        case '/': return {1,1,2,4,8,16,16};
        case '(': return {2,4,8,8,8,4,2};
        case ')': return {8,4,2,2,2,4,8};
        case '+': return {0,4,4,31,4,4,0};
        default: return {14,17,1,2,4,0,4};
    }
}
} // namespace

std::vector<UiDrawQuad> build_ui_draw_list(const UiLayout& layout) {
    std::vector<UiDrawQuad> commands;
    commands.reserve(layout.items.size());
    for (const auto& item : layout.items) {
        if (item.clipped_rect.width <= 0.0f ||
            item.clipped_rect.height <= 0.0f ||
            channel(item.color.a) == 0.0f)
            continue;
        if (item.kind == WidgetKind::Text && item.text.empty())
            continue;
        const bool background = item.kind != WidgetKind::Text;
        commands.push_back({item.id, item.clipped_rect, item.color,
                            item.text, background});
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
        if (draw.paint_background) for (auto y = y0; y < y1; ++y) {
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
        if (!draw.text.empty()) {
            const bool label_only = !draw.paint_background;
            const float lum = 0.2126f * src_r + 0.7152f * src_g + 0.0722f * src_b;
            const float ink = label_only ? 1.0f : (lum > 0.53f ? 0.04f : 1.0f);
            const float ink_r = label_only ? src_r : ink;
            const float ink_g = label_only ? src_g : ink;
            const float ink_b = label_only ? src_b : ink;
            const float ink_a = label_only ? src_a : 1.0f;
            const int scale = (x1 - x0 >= 100u && y1 - y0 >= 20u) ? 2 : 1;
            const auto letter_width = static_cast<std::uint32_t>(6 * scale);
            const auto start_x = x0 + static_cast<std::uint32_t>(2 * scale);
            const auto start_y = y0 + (y1 - y0 > static_cast<std::uint32_t>(7 * scale)
                ? (y1 - y0 - static_cast<std::uint32_t>(7 * scale)) / 2u : 0u);
            auto cursor = start_x;
            for (std::size_t i = 0; i < draw.text.size() && i < 256u; ++i) {
                if (cursor >= x1 || letter_width > x1 - cursor) break;
                const auto glyph_bits = glyph(
                    static_cast<unsigned char>(draw.text[i]));
                for (int gy = 0; gy < 7; ++gy) {
                    for (int gx = 0; gx < 5; ++gx) {
                        if (!(glyph_bits[gy] & (1u << (4 - gx)))) continue;
                        for (int sy = 0; sy < scale; ++sy) {
                            for (int sx = 0; sx < scale; ++sx) {
                                const auto x = cursor + static_cast<std::uint32_t>(gx * scale + sx);
                                const auto y = start_y + static_cast<std::uint32_t>(gy * scale + sy);
                                if (x >= x1 || y >= y1) continue;
                                auto* pixel = image.pixels.data() +
                                    (static_cast<std::size_t>(y) * width + x) * 4u;
                                const float dst_a = pixel[3] / 255.0f;
                                const float out_a = ink_a + dst_a * (1.0f - ink_a);
                                for (int ch = 0; ch < 3; ++ch) {
                                    const float dst = pixel[ch] / 255.0f;
                                    const float source = ch == 0 ? ink_r : (ch == 1 ? ink_g : ink_b);
                                    pixel[ch] = byte(out_a == 0.f ? 0.f :
                                        (source * ink_a + dst * dst_a * (1.f - ink_a)) / out_a);
                                }
                                pixel[3] = byte(out_a);
                            }
                        }
                    }
                }
                cursor += letter_width;
            }
        }
    }
    return image;
}

} // namespace nengine::ui

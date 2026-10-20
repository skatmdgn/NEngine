#pragma once
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "nengine/input/input_state.hpp"
#include "nengine/ui/canvas.hpp"
#include "nengine/ui/pointer_router.hpp"
#include "nengine/ui/rasterizer.hpp"
#include "nengine/ui/keyboard_router.hpp"

namespace nengine::player {
// Game-only mutable Canvas. Does not modify the author's .ncanvas source.
// Rendering is a separate Vulkan feature; this class owns interaction state.
class PlayerUiRuntime {
public:
    bool load(const std::filesystem::path& bundle_root,
              std::string* error = nullptr);
    void reset() noexcept;
    bool ready() const noexcept { return canvas_.has_value(); }
    const ui::UiCanvas* canvas() const noexcept {
        return canvas_ ? &*canvas_ : nullptr;
    }
    ui::UiCanvas* canvas() noexcept { return canvas_ ? &*canvas_ : nullptr; }
    ui::UiLayout layout(float width, float height) const;
    // Alpha-transparent overlay; caller composites after opaque World meshes.
    ui::UiRgbaImage rasterize(std::uint32_t width, std::uint32_t height) const;
    void bind_image_resolver(ui::UiImageResolver resolver) {
        images_ = std::move(resolver);
    }
    void update(float width, float height, const input::InputState& input,
                bool focused = true);
    ui::WidgetId find(std::string_view name) const noexcept;
    int query(ui::WidgetId id, int operation) const noexcept;
    int write(ui::WidgetId id, int operation, int value) noexcept;
    std::optional<std::string> get_text(ui::WidgetId id) const;
    bool set_text(ui::WidgetId id, std::string_view text);
    std::optional<std::string> image_texture(ui::WidgetId id) const;
    bool set_image_texture(ui::WidgetId id, std::string_view guid);
    std::size_t click_count() const noexcept { return clicked_.size(); }
    const std::vector<ui::UiPointerEvent>& events() const noexcept {
        return events_;
    }
private:
    std::optional<ui::UiCanvas> canvas_{};
    ui::UiImageResolver images_{};
    ui::UiPointerRouter pointer_{};
    ui::UiKeyboardRouter keyboard_{};
    std::unordered_set<ui::WidgetId> clicked_{};
    std::vector<ui::UiPointerEvent> events_{};
};
} // namespace nengine::player

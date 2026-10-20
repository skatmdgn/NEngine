#include "nengine/player/ui_runtime.hpp"
#include "nengine/ui/canvas_serialization.hpp"
#include "nengine/ui/rasterizer.hpp"
#include <cmath>
#include <fstream>
#include <system_error>

namespace nengine::player {
bool PlayerUiRuntime::load(const std::filesystem::path& bundle_root, std::string* error) {
    const auto content = bundle_root / "Content";
    const auto canvas_path = content / "Assets" / "UI" / "Main.ncanvas";
    std::error_code ec;
    const auto status = std::filesystem::symlink_status(canvas_path, ec);
    if (ec == std::errc::no_such_file_or_directory) ec.clear();
    // Unlike is_symlink(path, ec), symlink_status lets a nonexistent
    // optional UI directory remain valid on Linux and Windows alike.
    const auto safe_directory = [](const std::filesystem::path& path) {
        std::error_code directory_error;
        const auto directory_status =
            std::filesystem::symlink_status(path, directory_error);
        if (directory_error == std::errc::no_such_file_or_directory)
            directory_error.clear();
        return !directory_error &&
               !std::filesystem::is_symlink(directory_status);
    };
    const auto assets_dir = content / "Assets";
    const auto ui_dir = assets_dir / "UI";
    if (!safe_directory(content) || !safe_directory(assets_dir) ||
        !safe_directory(ui_dir) || std::filesystem::is_symlink(status)) {
        if (error) *error = "Player Canvas source is unsafe";
        return false;
    }
    if (!std::filesystem::exists(status)) {
        reset();
        if (error) error->clear();
        return true;
    }
    if (!std::filesystem::is_regular_file(canvas_path, ec) || ec ||
        std::filesystem::file_size(canvas_path, ec) > 4u * 1024u * 1024u || ec) {
        if (error) *error = "Player Canvas exceeds 4MiB or is not a regular file";
        return false;
    }
    const auto bundle = std::filesystem::canonical(bundle_root, ec);
    if (ec) return false;
    const auto file = std::filesystem::canonical(canvas_path, ec);
    const auto relative = file.lexically_relative(bundle);
    if (ec || relative.empty() || relative.is_absolute() ||
        *relative.begin() == "..") {
        if (error) *error = "Player Canvas escapes bundle root";
        return false;
    }
    ui::UiCanvas next;
    std::ifstream in(canvas_path, std::ios::binary);
    if (!in || !ui::read_canvas(in, next, error)) return false;
    canvas_ = std::move(next);
    pointer_.reset();
    keyboard_.reset();
    events_.clear();
    clicked_.clear();
    if (error) error->clear();
    return true;
}
void PlayerUiRuntime::reset() noexcept {
    canvas_.reset();
    pointer_.reset();
    keyboard_.reset();
    clicked_.clear();
    events_.clear();
    images_ = {};
}
ui::UiLayout PlayerUiRuntime::layout(float width, float height) const {
    return canvas_ ? canvas_->layout(width, height) : ui::UiLayout{};
}
ui::UiRgbaImage PlayerUiRuntime::rasterize(
    std::uint32_t width, std::uint32_t height) const {
    if (!canvas_ || width == 0u || height == 0u ||
        width > 4096u || height > 4096u) return {};
    const auto draw_commands = ui::build_ui_draw_list(
        canvas_->layout(static_cast<float>(width),
                        static_cast<float>(height)));
    if (draw_commands.empty()) return {};
    return ui::rasterize_ui(draw_commands, width, height, images_);
}
void PlayerUiRuntime::update(float width, float height,
                             const input::InputState& input, bool focused) {
    clicked_.clear();
    events_.clear();
    if (!canvas_ || !std::isfinite(width) || !std::isfinite(height) ||
        width <= 0.f || height <= 0.f || !focused) {
        pointer_.cancel_capture();
        keyboard_.reset();
        return;
    }
    // The same clipped/visibility-aware hit test is shared with Editor
    // Play Mode. UI clicks are ephemeral; a held button never repeats clicks.
    const auto visible = canvas_->layout(width, height);
    events_ = pointer_.update(visible, input);
    for (const auto& event : events_) {
        if (event.type == ui::UiPointerEventType::Down)
            keyboard_.set_focus(visible, event.target);
        if (event.type == ui::UiPointerEventType::Click)
            clicked_.insert(event.target);
    }
    auto keys = keyboard_.update(
        visible, input.held(input::Key::Tab),
        input.held(input::Key::Enter),
        input.held(input::Key::LeftShift) ||
            input.held(input::Key::RightShift),
        input.held(input::Key::Space),
        input.held(input::Key::Escape));
    for (const auto& event : keys)
        if (event.type == ui::UiPointerEventType::Click)
            clicked_.insert(event.target);
    events_.insert(events_.end(), keys.begin(), keys.end());
    if (events_.size() > 128u) events_.resize(128u);
}
ui::WidgetId PlayerUiRuntime::find(std::string_view name) const noexcept {
    if (!canvas_ || name.empty() || name.size() > 256u)
        return ui::invalid_widget;
    if (name.find('\0') != std::string_view::npos) return ui::invalid_widget;
    const auto named = canvas_->find_by_name(name);
    if (named != ui::invalid_widget) return named;
    return canvas_->find_by_text(name);
}
int PlayerUiRuntime::query(ui::WidgetId id, int operation) const noexcept {
    if (!canvas_) return -1;
    const auto* widget = canvas_->find(id);
    if (!widget) return -1;
    switch (operation) {
        case 0: return widget->visible ? 1 : 0;
        case 1: return widget->interactable ? 1 : 0;
        case 2: return clicked_.contains(id) ? 1 : 0;
        default: return -1;
    }
}
int PlayerUiRuntime::write(ui::WidgetId id, int operation, int value) noexcept {
    if (!canvas_ || value < 0 || value > 1 || !canvas_->find(id)) return -1;
    if (operation == 0) return canvas_->set_visible(id, value != 0) ? 1 : 0;
    if (operation == 1) return canvas_->set_interactable(id, value != 0) ? 1 : 0;
    return -1;
}
std::optional<std::string> PlayerUiRuntime::get_text(ui::WidgetId id) const {
    if (!canvas_) return std::nullopt;
    const auto* widget = canvas_->find(id);
    return widget ? std::optional<std::string>{widget->text} : std::nullopt;
}
bool PlayerUiRuntime::set_text(ui::WidgetId id, std::string_view value) {
    if (!canvas_ || value.size() > 4096u) return false;
    return canvas_->set_text(id, std::string(value));
}
std::optional<std::string> PlayerUiRuntime::image_texture(
    ui::WidgetId id) const {
    if (!canvas_) return std::nullopt;
    const auto* node = canvas_->find(id);
    if (!node || node->kind != ui::WidgetKind::Image) return std::nullopt;
    return node->image_texture.valid()
        ? node->image_texture.to_string() : std::string{};
}
bool PlayerUiRuntime::set_image_texture(
    ui::WidgetId id, std::string_view guid) {
    if (!canvas_ || guid.size() > 32u) return false;
    if (guid.empty()) return canvas_->set_image_texture(id, {});
    const auto parsed = assets::AssetGuid::parse(guid);
    return parsed && parsed->valid() &&
           canvas_->set_image_texture(id, *parsed);
}
} // namespace nengine::player

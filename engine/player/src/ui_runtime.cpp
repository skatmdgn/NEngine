#include "nengine/player/ui_runtime.hpp"
#include "nengine/ui/canvas_serialization.hpp"
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
}
ui::UiLayout PlayerUiRuntime::layout(float width, float height) const {
    return canvas_ ? canvas_->layout(width, height) : ui::UiLayout{};
}
void PlayerUiRuntime::update(float, float, const input::InputState&, bool) {
    clicked_.clear();
    events_.clear();
}
ui::WidgetId PlayerUiRuntime::find(std::string_view) const noexcept {
    return ui::invalid_widget;
}
int PlayerUiRuntime::query(ui::WidgetId, int) const noexcept { return -1; }
int PlayerUiRuntime::write(ui::WidgetId, int, int) noexcept { return -1; }
std::optional<std::string> PlayerUiRuntime::get_text(ui::WidgetId) const {
    return std::nullopt;
}
bool PlayerUiRuntime::set_text(ui::WidgetId, std::string_view) { return false; }
} // namespace nengine::player

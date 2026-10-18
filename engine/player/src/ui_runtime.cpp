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
    if (ec || std::filesystem::is_symlink(status)) {
        if (error) *error = "Player Canvas source is unsafe";
        return false;
    }
    if (!std::filesystem::exists(status)) {
        reset();
        if (error) error->clear();
        return true;
    }
    reset();
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

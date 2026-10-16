#include "nengine/player/assets.hpp"
#include <filesystem>
#include <utility>
namespace nengine::player {
bool PlayerAssets::open(const std::filesystem::path& bundle_root,
                        std::string* error) {
    std::error_code ec;
    const auto content = std::filesystem::canonical(
        bundle_root / "Content" / "Assets", ec);
    if (ec || !std::filesystem::is_directory(content)) {
        if (error) *error = "Player Content/Assets directory unavailable";
        return false;
    }
    assets::AssetDatabase next(content);
    const auto result = next.scan(false);
    for (const auto& message : result.messages) {
        if (message.severity == assets::AssetMessageSeverity::Error) {
            if (error) *error = message.message;
            return false;
        }
    }
    root_ = content;
    database_ = std::move(next);
    if (error) error->clear();
    return true;
}
void PlayerAssets::clear() {
    database_.clear();
    root_.clear();
}
std::optional<std::filesystem::path> PlayerAssets::source(
    assets::AssetGuid guid, std::string* error) const {
    if (root_.empty() || !guid.valid()) return std::nullopt;
    const auto* record = database_.find(guid);
    if (!record) return std::nullopt;
    std::error_code ec;
    if (std::filesystem::is_symlink(record->source_path, ec) || ec)
        return std::nullopt;
    const auto path = std::filesystem::canonical(record->source_path, ec);
    if (ec || !std::filesystem::is_regular_file(path)) return std::nullopt;
    const auto relative = path.lexically_relative(root_);
    if (relative.empty() || relative.is_absolute() ||
        *relative.begin() == "..") {
        if (error) *error = "Player asset escapes export root";
        return std::nullopt;
    }
    if (error) error->clear();
    return path;
}
} // namespace nengine::player

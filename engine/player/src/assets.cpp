#include "nengine/player/assets.hpp"
#include <filesystem>
#include <fstream>
#include <vector>
#include <cstdint>
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
std::optional<render::AnimationController> PlayerAssets::load_controller(
    assets::AssetGuid guid, std::string* error) const {
    const auto path = source(guid, error);
    if (!path || path->extension() != ".nanimctrl") return std::nullopt;
    std::error_code ec;
    const auto size = std::filesystem::file_size(*path, ec);
    if (ec || size > 4u * 1024u * 1024u) {
        if (error) *error = "animation controller exceeds player size limit";
        return std::nullopt;
    }
    std::ifstream input(*path, std::ios::binary);
    render::AnimationController controller;
    if (!input || !render::read_animation_controller(input, controller, error))
        return std::nullopt;
    if (error) error->clear();
    return controller;
}

std::optional<render::SpriteAnimationClip> PlayerAssets::load_sprite_clip(
    assets::AssetGuid guid, std::string* error) const {
    const auto path = source(guid, error);
    if (!path || path->extension() != ".nanim") return std::nullopt;
    std::error_code ec;
    const auto bytes = std::filesystem::file_size(*path, ec);
    if (ec || bytes > 4u * 1024u * 1024u) {
        if (error) *error = "sprite clip exceeds player size limit";
        return std::nullopt;
    }
    std::ifstream input(*path, std::ios::binary);
    render::SpriteAnimationClip clip;
    if (!input || !render::read_sprite_animation_clip(input, clip, error))
        return std::nullopt;
    if (error) error->clear();
    return clip;
}

std::optional<audio::AudioClipData> PlayerAssets::load_wav(
    assets::AssetGuid guid, std::string* error) const {
    const auto path = source(guid, error);
    if (!path || path->extension() != ".wav") return std::nullopt;
    std::ifstream stream(*path, std::ios::binary | std::ios::ate);
    if (!stream) return std::nullopt;
    const auto bytes = stream.tellg();
    if (bytes < 44 || bytes > 64 * 1024 * 1024) {
        if (error) *error = "WAV asset exceeds supported decode limit";
        return std::nullopt;
    }
    std::vector<std::uint8_t> buffer(static_cast<std::size_t>(bytes));
    stream.seekg(0);
    if (!stream.read(reinterpret_cast<char*>(buffer.data()),
                     static_cast<std::streamsize>(buffer.size())))
        return std::nullopt;
    audio::AudioClipData result;
    if (!audio::decode_wav(buffer, result, error))
        return std::nullopt;
    if (error) error->clear();
    return result;
}

} // namespace nengine::player

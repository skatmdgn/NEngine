#include "nengine/player/assets.hpp"
#include "nengine/player/subassets.hpp"
#include <filesystem>
#include <fstream>
#include <vector>
#include <cstdint>
#include <utility>
namespace nengine::player {
bool PlayerAssets::open(const std::filesystem::path& bundle_root,
                        std::string* error) {
    std::error_code ec;
    const auto base = std::filesystem::canonical(bundle_root, ec);
    if (ec || !std::filesystem::is_directory(base)) {
        if (error) *error = "Player bundle root unavailable";
        return false;
    }
    const auto content_path = base / "Content";
    const auto asset_path = content_path / "Assets";
    if (std::filesystem::is_symlink(content_path, ec) ||
        std::filesystem::is_symlink(asset_path, ec) || ec) {
        if (error) *error = "Player Content directories cannot be symlinks";
        return false;
    }
    const auto content = std::filesystem::canonical(asset_path, ec);
    if (ec || !std::filesystem::is_directory(content)) {
        if (error) *error = "Player Content/Assets directory unavailable";
        return false;
    }
    const auto rel = content.lexically_relative(base);
    if (rel.empty() || rel.is_absolute() || *rel.begin() == "..") {
        if (error) *error = "Player asset catalog escapes bundle root";
        return false;
    }
    // Neither the global cooked root nor an individual model cache may
    // redirect packaged generated resources outside the exported bundle.
    const auto cooked_root = content_path / "Cooked";
    std::error_code cooked_ec;
    const auto cooked_status = std::filesystem::symlink_status(cooked_root, cooked_ec);
    if (cooked_ec == std::errc::no_such_file_or_directory)
        cooked_ec.clear();
    if (cooked_ec || std::filesystem::is_symlink(cooked_status)) {
        if (error) *error = "unsafe cooked asset root";
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
    std::unordered_map<assets::AssetGuid, assets::CachedArtifactSet,
        assets::AssetGuidHash> next_subassets;
    for (const auto& record : next.records()) {
        const auto ext = record.source_path.extension();
        if (ext != ".glb" && ext != ".gltf" && ext != ".obj") continue;
        const auto folder = content_path / "Cooked" / record.guid.to_string();
        std::error_code folder_ec;
        const auto folder_status = std::filesystem::symlink_status(folder, folder_ec);
        if (folder_ec == std::errc::no_such_file_or_directory)
            folder_ec.clear();
        if (folder_ec || std::filesystem::is_symlink(folder_status)) {
            if (error) *error = "unsafe model cooked directory";
            return false;
        }
        std::vector<CookedSubasset> entries;
        if (!read_model_subassets(folder, entries, error)) return false;
        for (const auto& item : entries) {
            if (next.find(item.guid) || next_subassets.contains(item.guid)) {
                if (error) *error = "generated subasset GUID collides with source or another model";
                return false;
            }
            assets::CachedArtifactSet artifacts;
            artifacts.importer_id = item.importer_id;
            artifacts.importer_version = 1;
            artifacts.fingerprint =
                "Player-Generated-v1-" + record.guid.to_string() + "-" +
                item.guid.to_string();
            artifacts.artifacts = item.artifacts;
            next_subassets.emplace(item.guid, std::move(artifacts));
            if (next_subassets.size() > 8192u) {
                if (error) *error = "too many generated model subassets";
                return false;
            }
        }
    }
    root_ = content;
    database_ = std::move(next);
    generated_subassets_ = std::move(next_subassets);
    if (error) error->clear();
    return true;
}
std::optional<assets::CachedArtifactSet> PlayerAssets::cooked_artifacts(
    assets::AssetGuid guid) const {
    if (!guid.valid() || root_.empty()) return std::nullopt;
    if (const auto found = generated_subassets_.find(guid);
        found != generated_subassets_.end()) {
        std::error_code ec;
        const auto cooked_parent = root_.parent_path() / "Cooked";
        if (std::filesystem::is_symlink(cooked_parent, ec) || ec)
            return std::nullopt;
        const auto cooked_root =
            std::filesystem::canonical(cooked_parent, ec);
        if (ec) return std::nullopt;
        for (const auto& artifact : found->second.artifacts) {
            if (std::filesystem::is_symlink(artifact.path, ec) || ec ||
                !std::filesystem::is_regular_file(artifact.path, ec) || ec)
                return std::nullopt;
            const auto resolved = std::filesystem::canonical(artifact.path, ec);
            if (ec) return std::nullopt;
            const auto relative = resolved.lexically_relative(cooked_root);
            if (relative.empty() || relative.is_absolute() ||
                *relative.begin() == "..") return std::nullopt;
        }
        return found->second;
    }
    const auto* record = database_.find(guid);
    if (!record || !guid.valid() || root_.empty()) return std::nullopt;
    const auto folder = root_.parent_path() / "Cooked" / guid.to_string();
    std::error_code ec;
    if (std::filesystem::is_symlink(folder, ec) || ec)
        return std::nullopt;
    const auto extension = record->source_path.extension().string();
    const bool texture = extension == ".png" || extension == ".jpg" ||
        extension == ".jpeg" || extension == ".webp" ||
        extension == ".bmp" || extension == ".tga";
    const bool material = extension == ".nmat";
    const bool model = extension == ".glb" ||
        extension == ".gltf" || extension == ".obj";
    if (!texture && !material && !model) return std::nullopt;
    const auto descriptor = folder /
        (model ? "model.nasset" : "texture.nasset");
    const auto staged = folder / ("source" + extension);
    if (std::filesystem::is_symlink(staged, ec) || ec ||
        !std::filesystem::is_regular_file(staged, ec) || ec)
        return std::nullopt;
    if ((texture || model) &&
        (std::filesystem::is_symlink(descriptor, ec) || ec ||
         !std::filesystem::is_regular_file(descriptor, ec) || ec))
        return std::nullopt;
    const auto actual = std::filesystem::canonical(folder, ec);
    if (ec || actual != folder.lexically_normal()) return std::nullopt;
    assets::CachedArtifactSet artifacts;
    artifacts.importer_id = texture ? "NEngine.Texture" :
        (model ? "NEngine.Model" : "NEngine.Material");
    artifacts.importer_version = 1;
    artifacts.fingerprint = "Player-Cooked-v1-" + guid.to_string();
    artifacts.artifacts.push_back({staged, "source"});
    if (texture)
        artifacts.artifacts.push_back({descriptor, "texture-descriptor"});
    if (model) {
        artifacts.artifacts.push_back({descriptor, "model-descriptor"});
        for (std::filesystem::recursive_directory_iterator it(folder, ec), end;
             it != end && !ec; it.increment(ec)) {
            if (it->is_symlink(ec) || ec) return std::nullopt;
            if (it->is_directory(ec)) {
                if (it->path().filename() == "subassets")
                    it.disable_recursion_pending();
                continue;
            }
            if (ec || !it->is_regular_file(ec)) return std::nullopt;
            const auto asset = it->path();
            if (asset == staged || asset == descriptor ||
                asset.filename() == "subassets.nasset") continue;
            const auto relative = asset.lexically_relative(folder);
            if (relative.empty() || relative.is_absolute() ||
                *relative.begin() == "..") return std::nullopt;
            if (asset.filename() == "model-materials.nasset") {
                artifacts.artifacts.push_back({asset, "model-material-map"});
                continue;
            }
            const auto role = extension == ".obj"
                ? (asset.extension() == ".mtl"
                    ? "obj-mtl-sidecar" : "obj-texture-sidecar")
                : "model-sidecar";
            artifacts.artifacts.push_back({asset, role});
            if (artifacts.artifacts.size() > 1024u) return std::nullopt;
        }
        if (ec) return std::nullopt;
    }
    return artifacts;
}

void PlayerAssets::clear() {
    generated_subassets_.clear();
    database_.clear();
    root_.clear();
}
std::optional<std::filesystem::path> PlayerAssets::source(
    assets::AssetGuid guid, std::string* error) const {
    if (root_.empty() || !guid.valid()) return std::nullopt;
    const auto* record = database_.find(guid);
    if (!record) {
        const auto generated = cooked_artifacts(guid);
        if (!generated) return std::nullopt;
        for (const auto& artifact : generated->artifacts) {
            if (artifact.role == "source") {
                if (error) error->clear();
                return artifact.path;
            }
        }
        return std::nullopt;
    }
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
    if (!path || (path->extension() != ".nanim" &&
                  path->extension() != ".nspriteanim")) return std::nullopt;
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

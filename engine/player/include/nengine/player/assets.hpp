#pragma once
#include <filesystem>
#include <optional>
#include <string>
#include "nengine/assets/asset_database.hpp"
namespace nengine::player {
// A read-only GUID catalog for exported Source Assets; unlike ProjectSession,
// this never writes missing .meta files or runs the editor import pipeline.
class PlayerAssets {
public:
    bool open(const std::filesystem::path& bundle_root,
              std::string* error = nullptr);
    void clear();
    std::optional<std::filesystem::path> source(
        assets::AssetGuid guid, std::string* error = nullptr) const;
    std::size_t size() const noexcept { return database_.size(); }
private:
    std::filesystem::path root_{};
    assets::AssetDatabase database_{};
};
} // namespace nengine::player

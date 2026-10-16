#pragma once

#include <cstddef>
#include <filesystem>
#include <string>

#include "nengine/core/scene.hpp"
#include "nengine/player/bundle.hpp"

namespace nengine::player {

// Isolated player World bootstrap. Never depends on the editor UI/model.
class PlayerRuntime {
public:
    bool load(const std::filesystem::path& bundle_root,
              std::string* error = nullptr);
    void unload() noexcept;
    const core::World& world() const noexcept { return world_; }
    core::World& world() noexcept { return world_; }
    const BundleManifest& manifest() const noexcept { return manifest_; }
    bool ready() const noexcept { return ready_; }
    std::size_t entity_count() const noexcept { return world_.size(); }
private:
    core::World world_{};
    BundleManifest manifest_{};
    bool ready_{false};
};

} // namespace nengine::player

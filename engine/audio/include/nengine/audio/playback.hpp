#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <unordered_set>

#include "nengine/assets/asset_guid.hpp"
#include "nengine/core/world.hpp"

namespace nengine::audio {

using AudioClipDurationResolver =
    std::function<
        std::optional<float>(
            assets::AssetGuid)>;

struct AudioPlaybackStats {
    std::size_t started{0};
    std::size_t advanced{0};
    std::size_t looped{0};
    std::size_t stopped{0};
    std::size_t unresolved{0};
};

class AudioPlaybackSystem {
public:
    AudioPlaybackStats update(
        core::World& world,
        float delta_seconds,
        const AudioClipDurationResolver&
            duration_resolver = {});

    void reset() noexcept;

    std::size_t initialized_source_count()
        const noexcept {
        return initialized_.size();
    }

private:
    std::unordered_set<
        core::Entity::value_type>
        initialized_{};
};

} // namespace nengine::audio

#pragma once

#include <cstddef>
#include <vector>

#include "nengine/assets/asset_guid.hpp"
#include "nengine/core/entity.hpp"
#include "nengine/core/math.hpp"
#include "nengine/core/world.hpp"

namespace nengine::audio {

struct AudioListenerState {
    core::Entity entity{
        core::Entity::invalid()};
    core::Vec3 position{};
    core::Quat rotation{};
    float volume{1.0f};
};

struct AudioSourceMixState {
    core::Entity entity{
        core::Entity::invalid()};
    assets::AssetGuid clip{};
    core::Vec3 position{};
    bool spatialized{false};
    bool playing{false};
    bool loop{false};
    float time_seconds{0.0f};
    float pitch{1.0f};
    float left_gain{0.0f};
    float right_gain{0.0f};
};

struct AudioMixSnapshot {
    bool has_listener{false};
    AudioListenerState listener{};
    std::vector<AudioSourceMixState>
        sources{};
};

AudioMixSnapshot build_mix_snapshot(
    const core::World& world);

} // namespace nengine::audio

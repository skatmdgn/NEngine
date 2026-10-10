#pragma once

#include "nengine/assets/asset_guid.hpp"
#include "nengine/core/component_registry.hpp"

namespace nengine::audio {

struct AudioSource {
    bool enabled{true};
    assets::AssetGuid clip{};
    bool play_on_awake{true};
    bool loop{false};
    bool spatialize{false};
    float volume{1.0f};
    float pitch{1.0f};
    float pan_stereo{0.0f};

    // Runtime-only playback state.
    bool playing{false};
    float time_seconds{0.0f};
};

struct AudioListener {
    bool enabled{true};
    float volume{1.0f};
};

inline core::ComponentTypeId audio_source_type() noexcept {
    return core::ComponentRegistry::stable_id(
        "NEngine.AudioSource");
}

inline core::ComponentTypeId audio_listener_type() noexcept {
    return core::ComponentRegistry::stable_id(
        "NEngine.AudioListener");
}

} // namespace nengine::audio

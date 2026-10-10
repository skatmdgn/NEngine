#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

#include "nengine/assets/asset_guid.hpp"
#include "nengine/audio/audio_clip.hpp"
#include "nengine/audio/mix_snapshot.hpp"

namespace nengine::audio {

using AudioClipResolver =
    std::function<
        const AudioClipData*(
            assets::AssetGuid)>;

struct AudioRenderStats {
    std::size_t sources_considered{0};
    std::size_t sources_mixed{0};
    std::size_t unresolved_clips{0};
    std::size_t frames_rendered{0};
};

AudioRenderStats render_stereo_mix(
    const AudioMixSnapshot& snapshot,
    const AudioClipResolver& resolver,
    std::uint32_t output_sample_rate,
    std::size_t frame_count,
    std::vector<float>& interleaved_stereo);

} // namespace nengine::audio

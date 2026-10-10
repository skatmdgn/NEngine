#pragma once

#include <string>
#include <unordered_map>

#include "nengine/assets/asset_guid.hpp"
#include "nengine/assets/import_pipeline.hpp"
#include "nengine/audio/audio_clip.hpp"

namespace nengine::audio {

class AudioClipCache {
public:
    const AudioClipData* load(
        assets::AssetGuid guid,
        const assets::CachedArtifactSet& artifacts,
        std::string* error = nullptr);

    const AudioClipData* find(
        assets::AssetGuid guid) const noexcept;

    void clear() noexcept;

private:
    struct Entry {
        std::string fingerprint{};
        AudioClipData clip{};
    };

    std::unordered_map<
        assets::AssetGuid,
        Entry,
        assets::AssetGuidHash>
        entries_{};
};

} // namespace nengine::audio

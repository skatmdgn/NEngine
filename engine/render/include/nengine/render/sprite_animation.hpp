#pragma once

#include <cstddef>
#include <functional>
#include <istream>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "nengine/assets/asset_guid.hpp"
#include "nengine/assets/import_pipeline.hpp"
#include "nengine/core/world.hpp"

namespace nengine::render {

struct SpriteAnimationFrame {
    assets::AssetGuid texture{};
    float duration_seconds{0.1f};
};

struct SpriteAnimationClip {
    std::vector<SpriteAnimationFrame> frames{};

    bool valid() const noexcept;
    float duration_seconds() const noexcept;
};

bool read_sprite_animation_clip(
    std::istream& input,
    SpriteAnimationClip& clip,
    std::string* error = nullptr);

std::optional<SpriteAnimationClip>
resolve_sprite_animation_clip(
    assets::AssetGuid guid,
    const assets::CachedArtifactSet& artifacts,
    std::string* error = nullptr);

assets::ImportResult sprite_animation_source_importer(
    const assets::ImportContext& context);

std::optional<std::size_t>
sample_sprite_animation_frame(
    const SpriteAnimationClip& clip,
    float time_seconds,
    bool loop) noexcept;

class SpriteAnimationClipCache {
public:
    const SpriteAnimationClip* load(
        assets::AssetGuid guid,
        const assets::CachedArtifactSet& artifacts,
        std::string* error = nullptr);

    const SpriteAnimationClip* find(
        assets::AssetGuid guid) const noexcept;

    void clear() noexcept;

private:
    struct Entry {
        std::string fingerprint{};
        SpriteAnimationClip clip{};
    };

    std::unordered_map<
        assets::AssetGuid,
        Entry,
        assets::AssetGuidHash> entries_{};
};

using SpriteAnimationArtifactResolver =
    std::function<
        std::optional<assets::CachedArtifactSet>(
            assets::AssetGuid)>;

struct SpriteAnimationUpdateStats {
    std::size_t advanced{0};
    std::size_t sampled{0};
    std::size_t unresolved{0};
};

SpriteAnimationUpdateStats update_sprite_animators(
    core::World& world,
    float delta_seconds,
    SpriteAnimationClipCache& cache,
    const SpriteAnimationArtifactResolver& resolver,
    std::string* error = nullptr);

} // namespace nengine::render

#include "nengine/render/sprite_animation.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <unordered_set>
#include <utility>

#include "nengine/assets/builtin_processors.hpp"
#include "nengine/render/components.hpp"

namespace nengine::render {
namespace {

void set_error(
    std::string* error,
    std::string message) {

    if (error) {
        *error = std::move(message);
    }
}

const assets::ImportArtifact*
find_source(
    const assets::CachedArtifactSet& artifacts) {

    const auto it =
        std::find_if(
            artifacts.artifacts.begin(),
            artifacts.artifacts.end(),
            [](const auto& artifact) {
                return artifact.role ==
                    "source";
            });

    return it ==
        artifacts.artifacts.end()
        ? nullptr
        : &*it;
}

} // namespace

bool SpriteAnimationClip::valid() const noexcept {
    if (frames.empty()) {
        return false;
    }

    for (const auto& frame :
         frames) {

        if (!frame.texture.valid() ||
            !std::isfinite(
                frame.duration_seconds) ||
            frame.duration_seconds <=
                0.0f) {

            return false;
        }
    }

    return true;
}

float SpriteAnimationClip::duration_seconds()
    const noexcept {

    double total = 0.0;

    for (const auto& frame :
         frames) {
        total +=
            frame.duration_seconds;
    }

    return static_cast<float>(
        std::min(
            total,
            static_cast<double>(
                std::numeric_limits<float>::max())));
}

bool read_sprite_animation_clip(
    std::istream& input,
    SpriteAnimationClip& clip,
    std::string* error) {

    clip = {};

    std::string token;
    std::uint32_t version = 0u;

    if (!(input >> token >> version) ||
        token !=
            "NENGINE_SPRITE_ANIMATION" ||
        version != 1u) {

        set_error(
            error,
            "invalid NEngine sprite animation header");
        return false;
    }

    std::size_t frame_count = 0u;

    if (!(input >> token >> frame_count) ||
        token != "FRAMES" ||
        frame_count == 0u ||
        frame_count > 65536u) {

        set_error(
            error,
            "invalid sprite animation frame count");
        return false;
    }

    clip.frames.reserve(
        frame_count);

    for (std::size_t i = 0u;
         i < frame_count;
         ++i) {

        std::string guid_text;
        float duration = 0.0f;

        if (!(input >> token) ||
            token != "FRAME" ||
            !(input >>
                std::quoted(
                    guid_text)) ||
            !(input >> duration)) {

            set_error(
                error,
                "malformed sprite animation FRAME record");
            return false;
        }

        const auto guid =
            assets::AssetGuid::parse(
                guid_text);

        if (!guid ||
            !guid->valid() ||
            !std::isfinite(duration) ||
            duration <= 0.0f) {

            set_error(
                error,
                "sprite animation frame has invalid Texture AssetGuid or duration");
            return false;
        }

        clip.frames.push_back({
            *guid,
            duration
        });
    }

    if (!(input >> token) ||
        token !=
            "END_SPRITE_ANIMATION") {

        set_error(
            error,
            "sprite animation terminator is missing");
        return false;
    }

    if (!clip.valid()) {
        set_error(
            error,
            "sprite animation clip is invalid");
        return false;
    }

    return true;
}

std::optional<SpriteAnimationClip>
resolve_sprite_animation_clip(
    assets::AssetGuid guid,
    const assets::CachedArtifactSet& artifacts,
    std::string* error) {

    if (!guid.valid()) {
        set_error(
            error,
            "sprite animation AssetGuid is invalid");
        return std::nullopt;
    }

    const auto* source =
        find_source(
            artifacts);

    if (!source) {
        set_error(
            error,
            "sprite animation cached source artifact is missing");
        return std::nullopt;
    }

    std::ifstream input(
        source->path,
        std::ios::binary);

    if (!input) {
        set_error(
            error,
            "could not open sprite animation source");
        return std::nullopt;
    }

    SpriteAnimationClip clip;

    if (!read_sprite_animation_clip(
            input,
            clip,
            error)) {
        return std::nullopt;
    }

    return clip;
}

assets::ImportResult
sprite_animation_source_importer(
    const assets::ImportContext& context) {

    auto result =
        assets::copy_source_importer(
            context);

    if (!result.success) {
        return result;
    }

    assets::CachedArtifactSet staged;
    staged.fingerprint =
        "sprite-animation-stage";
    staged.importer_id =
        context.importer
            ? context.importer->id
            : "NEngine.SpriteAnimation";
    staged.importer_version =
        context.importer
            ? context.importer->version
            : 1u;
    staged.artifacts =
        result.artifacts;

    const auto clip =
        resolve_sprite_animation_clip(
            context.asset
                ? context.asset->guid
                : assets::AssetGuid{},
            staged,
            &result.message);

    if (!clip) {
        result.success = false;
        return result;
    }

    std::unordered_set<
        assets::AssetGuid,
        assets::AssetGuidHash>
        seen;

    for (const auto& frame :
         clip->frames) {

        if (seen.insert(
                frame.texture)
                .second) {
            result.dependencies.push_back(
                frame.texture);
        }
    }

    result.message =
        "sprite animation staged with " +
        std::to_string(
            clip->frames.size()) +
        " frame(s)";

    return result;
}

std::optional<std::size_t>
sample_sprite_animation_frame(
    const SpriteAnimationClip& clip,
    float time_seconds,
    bool loop) noexcept {

    if (!clip.valid() ||
        !std::isfinite(time_seconds)) {
        return std::nullopt;
    }

    const auto total =
        clip.duration_seconds();

    if (!(total > 0.0f) ||
        !std::isfinite(total)) {
        return std::nullopt;
    }

    double time =
        std::max(
            0.0,
            static_cast<double>(
                time_seconds));

    if (loop) {
        time =
            std::fmod(
                time,
                static_cast<double>(
                    total));
    } else if (
        time >= total) {
        return
            clip.frames.size() -
            1u;
    }

    double cursor = 0.0;

    for (std::size_t i = 0u;
         i < clip.frames.size();
         ++i) {

        cursor +=
            clip.frames[i]
                .duration_seconds;

        if (time <
                cursor ||
            i + 1u ==
                clip.frames.size()) {
            return i;
        }
    }

    return
        clip.frames.size() -
        1u;
}

const SpriteAnimationClip*
SpriteAnimationClipCache::load(
    assets::AssetGuid guid,
    const assets::CachedArtifactSet& artifacts,
    std::string* error) {

    const auto existing =
        entries_.find(guid);

    if (existing !=
            entries_.end() &&
        existing->second.fingerprint ==
            artifacts.fingerprint &&
        existing->second.clip.valid()) {

        return
            &existing->second.clip;
    }

    const auto resolved =
        resolve_sprite_animation_clip(
            guid,
            artifacts,
            error);

    if (!resolved) {
        return nullptr;
    }

    Entry entry;
    entry.fingerprint =
        artifacts.fingerprint;
    entry.clip =
        std::move(
            *resolved);

    const auto [it, inserted] =
        entries_.insert_or_assign(
            guid,
            std::move(
                entry));

    (void)inserted;

    return
        &it->second.clip;
}

const SpriteAnimationClip*
SpriteAnimationClipCache::find(
    assets::AssetGuid guid) const noexcept {

    const auto it =
        entries_.find(guid);

    return
        it == entries_.end()
        ? nullptr
        : &it->second.clip;
}

void SpriteAnimationClipCache::clear() noexcept {
    entries_.clear();
}

SpriteAnimationUpdateStats update_sprite_animators(
    core::World& world,
    float delta_seconds,
    SpriteAnimationClipCache& cache,
    const SpriteAnimationArtifactResolver& resolver,
    std::string* error) {

    SpriteAnimationUpdateStats stats;

    if (!std::isfinite(delta_seconds) ||
        delta_seconds < 0.0f) {

        set_error(
            error,
            "sprite animation delta must be finite and non-negative");
        return stats;
    }

    for (const auto entity :
         world.entities()) {

        if (!world.active(entity)) {
            continue;
        }

        auto* animator =
            world.get_component<SpriteAnimator>(
                entity,
                sprite_animator_type());

        auto* renderer =
            world.get_component<SpriteRenderer>(
                entity,
                sprite_renderer_type());

        if (!animator ||
            !renderer ||
            !animator->enabled ||
            !renderer->enabled ||
            !animator->clip.valid()) {
            continue;
        }

        const SpriteAnimationClip* clip =
            cache.find(
                animator->clip);

        if (!clip) {
            if (!resolver) {
                ++stats.unresolved;
                continue;
            }

            const auto artifacts =
                resolver(
                    animator->clip);

            if (!artifacts) {
                ++stats.unresolved;
                continue;
            }

            std::string clip_error;

            clip =
                cache.load(
                    animator->clip,
                    *artifacts,
                    &clip_error);

            if (!clip) {
                ++stats.unresolved;

                if (error &&
                    !clip_error.empty()) {
                    *error =
                        std::move(
                            clip_error);
                }

                continue;
            }
        }

        if (animator->playing &&
            animator->speed > 0.0f &&
            delta_seconds > 0.0f) {

            const double advanced =
                static_cast<double>(
                    animator->time_seconds) +
                static_cast<double>(
                    delta_seconds) *
                static_cast<double>(
                    animator->speed);

            animator->time_seconds =
                static_cast<float>(
                    std::min(
                        advanced,
                        static_cast<double>(
                            std::numeric_limits<
                                float>::max())));

            ++stats.advanced;
        }

        const auto frame_index =
            sample_sprite_animation_frame(
                *clip,
                animator->time_seconds,
                animator->loop);

        if (!frame_index) {
            ++stats.unresolved;
            continue;
        }

        renderer->texture =
            clip->frames[
                *frame_index]
                .texture;

        ++stats.sampled;

        if (!animator->loop &&
            animator->playing &&
            animator->time_seconds >=
                clip->duration_seconds()) {

            animator->time_seconds =
                clip->duration_seconds();
            animator->playing =
                false;
        }
    }

    return stats;
}

} // namespace nengine::render

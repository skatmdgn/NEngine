#include "nengine/audio/clip_cache.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <vector>
#include <utility>

namespace nengine::audio {
namespace {

void set_error(
    std::string* error,
    std::string message) {

    if (error) {
        *error =
            std::move(message);
    }
}

std::string lowercase(
    std::string value) {

    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](unsigned char ch) {
            return static_cast<char>(
                std::tolower(ch));
        });

    return value;
}

} // namespace

const AudioClipData* AudioClipCache::load(
    assets::AssetGuid guid,
    const assets::CachedArtifactSet& artifacts,
    std::string* error) {

    if (!guid.valid()) {
        set_error(
            error,
            "audio clip GUID is invalid");
        return nullptr;
    }

    const auto existing =
        entries_.find(guid);

    if (existing != entries_.end() &&
        existing->second.fingerprint ==
            artifacts.fingerprint) {

        if (error) {
            error->clear();
        }

        return &existing->second.clip;
    }

    const assets::ImportArtifact*
        source = nullptr;

    for (const auto& artifact :
         artifacts.artifacts) {

        if (artifact.role == "source") {
            source = &artifact;
            break;
        }
    }

    if (!source ||
        source->path.empty()) {

        set_error(
            error,
            "audio cache has no staged source artifact");
        return nullptr;
    }

    const auto extension =
        lowercase(
            source->path
                .extension()
                .string());

    if (extension != ".wav") {
        set_error(
            error,
            "runtime audio decode currently supports WAV only");
        return nullptr;
    }

    std::ifstream input(
        source->path,
        std::ios::binary);

    if (!input) {
        set_error(
            error,
            "could not open cached WAV source");
        return nullptr;
    }

    std::vector<std::uint8_t>
        bytes{
            std::istreambuf_iterator<char>(
                input),
            std::istreambuf_iterator<char>()};

    if (!input.good() &&
        !input.eof()) {

        set_error(
            error,
            "failed reading cached WAV source");
        return nullptr;
    }

    AudioClipData clip;
    std::string decode_error;

    if (!decode_wav(
            bytes,
            clip,
            &decode_error)) {

        set_error(
            error,
            "cached WAV decode failed: " +
                decode_error);
        return nullptr;
    }

    Entry entry;
    entry.fingerprint =
        artifacts.fingerprint;
    entry.clip =
        std::move(clip);

    auto [it, inserted] =
        entries_.insert_or_assign(
            guid,
            std::move(entry));

    (void)inserted;

    if (error) {
        error->clear();
    }

    return &it->second.clip;
}

const AudioClipData* AudioClipCache::find(
    assets::AssetGuid guid) const noexcept {

    const auto it =
        entries_.find(guid);

    return it == entries_.end()
        ? nullptr
        : &it->second.clip;
}

void AudioClipCache::clear() noexcept {
    entries_.clear();
}

} // namespace nengine::audio

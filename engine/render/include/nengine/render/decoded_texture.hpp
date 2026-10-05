#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "nengine/assets/asset_guid.hpp"
#include "nengine/assets/import_pipeline.hpp"
#include "nengine/render/asset_resources.hpp"

namespace nengine::render {

enum class DecodedTextureColorSpace : std::uint8_t {
    Linear,
    SRgb,
};

struct DecodedTextureData {
    std::uint32_t width{0};
    std::uint32_t height{0};
    DecodedTextureColorSpace color_space{
        DecodedTextureColorSpace::SRgb};
    std::vector<std::uint8_t> rgba8{};

    bool valid() const noexcept {
        if (width == 0 || height == 0) {
            return false;
        }

        const auto expected =
            static_cast<std::uint64_t>(width) *
            static_cast<std::uint64_t>(height) *
            4u;

        return expected ==
            static_cast<std::uint64_t>(
                rgba8.size());
    }
};

bool decode_texture_rgba8(
    const ResolvedTextureAsset& asset,
    DecodedTextureData& output,
    std::string* error = nullptr);

class DecodedTextureCache {
public:
    const DecodedTextureData* load(
        assets::AssetGuid guid,
        const assets::CachedArtifactSet& artifacts,
        std::string* error = nullptr);

    const DecodedTextureData* find(
        assets::AssetGuid guid) const noexcept;

    bool erase(
        assets::AssetGuid guid) noexcept;

    void clear() noexcept;

    std::size_t size() const noexcept {
        return entries_.size();
    }

private:
    struct Entry {
        std::string fingerprint{};
        DecodedTextureData texture{};
    };

    std::unordered_map<
        assets::AssetGuid,
        Entry,
        assets::AssetGuidHash> entries_{};
};

} // namespace nengine::render

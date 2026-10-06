#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>

#include "nengine/assets/asset_guid.hpp"
#include "nengine/assets/import_pipeline.hpp"
#include "nengine/render/mesh_data.hpp"

namespace nengine::render {

class DecodedMeshCache {
public:
    const MeshData* load(
        assets::AssetGuid guid,
        const assets::CachedArtifactSet& artifacts,
        std::string* error = nullptr);

    const MeshData* find(
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
        MeshData mesh{};
    };

    std::unordered_map<
        assets::AssetGuid,
        Entry,
        assets::AssetGuidHash> entries_{};
};

} // namespace nengine::render

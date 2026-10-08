#include "nengine/render/decoded_mesh.hpp"

#include <algorithm>
#include <cctype>
#include <utility>

#include "nengine/render/asset_resources.hpp"
#include "nengine/render/gltf_mesh.hpp"
#include "nengine/render/obj_mesh.hpp"

namespace nengine::render {
namespace {

void set_error(
    std::string* error,
    std::string message) {

    if (error) {
        *error = std::move(message);
    }
}

std::string normalized_format(
    std::string format) {

    std::transform(
        format.begin(),
        format.end(),
        format.begin(),
        [](unsigned char ch) {
            return static_cast<char>(
                std::tolower(ch));
        });

    if (!format.empty() &&
        format.front() == '.') {
        format.erase(
            format.begin());
    }

    return format;
}

} // namespace

const MeshData*
DecodedMeshCache::load(
    assets::AssetGuid guid,
    const assets::CachedArtifactSet& artifacts,
    std::string* error) {

    if (!guid.valid()) {
        set_error(
            error,
            "mesh cache requires a valid AssetGuid");
        return nullptr;
    }

    const auto existing =
        entries_.find(guid);

    if (existing !=
            entries_.end() &&
        existing->second.fingerprint ==
            artifacts.fingerprint &&
        existing->second.mesh.valid()) {

        return &existing->second.mesh;
    }

    if (existing !=
        entries_.end()) {
        entries_.erase(existing);
    }

    const auto resolved =
        resolve_model_asset(
            guid,
            artifacts,
            error);

    if (!resolved) {
        return nullptr;
    }

    const auto format =
        normalized_format(
            resolved->metadata.format);

    MeshData decoded;

    if (format == "gltf" ||
        format == "glb") {

        if (!decode_gltf_mesh(
                *resolved,
                decoded,
                error)) {
            return nullptr;
        }
    } else if (
        format == "obj") {

        if (!decode_obj_mesh(
                *resolved,
                decoded,
                error)) {
            return nullptr;
        }
    } else {
        set_error(
            error,
            "mesh decoder is not implemented for model format: " +
                format);
        return nullptr;
    }

    if (!decoded.valid()) {
        set_error(
            error,
            "decoded MeshData is invalid");
        return nullptr;
    }

    Entry entry;
    entry.fingerprint =
        artifacts.fingerprint;
    entry.mesh =
        std::move(decoded);

    const auto [it, inserted] =
        entries_.emplace(
            guid,
            std::move(entry));

    if (!inserted) {
        set_error(
            error,
            "decoded mesh cache insertion failed");
        return nullptr;
    }

    return &it->second.mesh;
}

const MeshData*
DecodedMeshCache::find(
    assets::AssetGuid guid) const noexcept {

    const auto it =
        entries_.find(guid);

    return it ==
        entries_.end()
        ? nullptr
        : &it->second.mesh;
}

bool DecodedMeshCache::erase(
    assets::AssetGuid guid) noexcept {

    return entries_.erase(guid) != 0u;
}

void DecodedMeshCache::clear() noexcept {
    entries_.clear();
}

} // namespace nengine::render

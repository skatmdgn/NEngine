#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace nengine::assets {

// External files referenced by the top-level glTF 2.0 buffers/images arrays.
// Data URIs and GLB internal BIN/image bufferViews need no sidecar.
// Only existing, regular files beneath the source's directory are accepted.
struct GltfSidecar {
    std::filesystem::path relative_path{};
    std::filesystem::path source_path{};
};

// Resolve an external glTF URI to an existing regular file beneath the
// model directory. Percent-encoded UTF-8/local path bytes are decoded,
// URI schemes/query/fragment and traversal are rejected, and canonical
// paths are checked so symlinks cannot escape the model directory.
std::optional<GltfSidecar> resolve_gltf_sidecar(
    const std::filesystem::path& source_gltf,
    std::string_view uri,
    std::string* error = nullptr);

// Fails closed on malformed JSON, unsafe URI, missing or escaping sidecars.
bool collect_gltf_sidecars(
    const std::filesystem::path& source_gltf,
    std::vector<GltfSidecar>& sidecars,
    std::string* error = nullptr);

// Appends sidecar sizes and write timestamps so a changed .bin/.png source
// invalidates its parent's staged model import even if the .gltf is unchanged.
std::string gltf_sidecar_fingerprint(
    const std::filesystem::path& source_gltf);

} // namespace nengine::assets

#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace nengine::assets {

enum class ObjSidecarKind {
    MaterialLibrary,
    DiffuseTexture,
};

struct ObjSidecar {
    std::filesystem::path relative_path{};
    std::filesystem::path source_path{};
    ObjSidecarKind kind{
        ObjSidecarKind::MaterialLibrary};
};

// Resolve one local OBJ/MTL dependency using the same sandbox policy used
// by sidecar staging. root_directory is the OBJ directory; base_directory is
// the directory containing the directive (OBJ or MTL).
std::optional<ObjSidecar> resolve_obj_sidecar(
    const std::filesystem::path& root_directory,
    const std::filesystem::path& base_directory,
    std::string_view raw_path,
    ObjSidecarKind kind,
    std::string* error = nullptr);

// Collect local OBJ material-library dependencies and the diffuse textures
// referenced by map_Kd. All dependencies must resolve to regular files below
// the OBJ source directory. Relative "."/".." segments are allowed for common
// MTL layouts, but canonical resolution must remain inside the OBJ directory.
bool collect_obj_sidecars(
    const std::filesystem::path& source_obj,
    std::vector<ObjSidecar>& sidecars,
    std::string* error = nullptr);

// Content-based fingerprint of every tracked .mtl/map_Kd sidecar.
std::string obj_sidecar_fingerprint(
    const std::filesystem::path& source_obj);

} // namespace nengine::assets

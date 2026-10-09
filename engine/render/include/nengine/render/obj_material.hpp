#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "nengine/render/asset_resources.hpp"

namespace nengine::render {

struct ObjCookedMaterialSource {
    std::uint32_t slot{0};
    std::string name{};
    std::array<float, 3> diffuse{
        1.0f, 1.0f, 1.0f};
    float alpha{1.0f};
    std::optional<std::filesystem::path>
        diffuse_texture{};

    bool valid() const noexcept {
        return !name.empty();
    }
};

// Resolve MTL definitions for the usemtl slots referenced by one OBJ.
// Undefined usemtl names are omitted rather than treated as fatal so geometry
// remains renderable with the engine fallback material.
bool load_obj_material_sources(
    const ResolvedModelAsset& asset,
    std::vector<ObjCookedMaterialSource>& materials,
    std::string* error = nullptr);

} // namespace nengine::render

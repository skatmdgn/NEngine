#pragma once

#include <cstdint>
#include "nengine/render/material_asset.hpp"

namespace nengine::render {

// SHDR-004/006: one source of truth for Editor and Player material variants.
// Missing materials resolve to an explicit opaque/double-sided-false fallback.
// Existing Vulkan pipelines cover Opaque/Blend * Cull/Double-sided.
// AlphaMask currently shares the opaque shader and requires future dedicated
// alpha-discard support; never report that path as fully implemented.
enum class MaterialPipelineVariant : std::uint8_t {
    OpaqueCull, OpaqueDoubleSided, BlendCull, BlendDoubleSided
};
inline MaterialPipelineVariant select_material_variant(
    const MaterialAssetData* material) noexcept {
    if(!material)return MaterialPipelineVariant::OpaqueCull;
    const bool blend=material->alpha_mode==MaterialAlphaMode::Blend;
    return blend?(material->double_sided?
        MaterialPipelineVariant::BlendDoubleSided:
        MaterialPipelineVariant::BlendCull):
        (material->double_sided?
        MaterialPipelineVariant::OpaqueDoubleSided:
        MaterialPipelineVariant::OpaqueCull);
}
inline constexpr bool variant_is_blended(MaterialPipelineVariant v) noexcept {
    return v==MaterialPipelineVariant::BlendCull ||
           v==MaterialPipelineVariant::BlendDoubleSided;
}
inline constexpr bool variant_is_double_sided(MaterialPipelineVariant v) noexcept {
    return v==MaterialPipelineVariant::OpaqueDoubleSided ||
           v==MaterialPipelineVariant::BlendDoubleSided;
}

} // namespace nengine::render

#pragma once
#include <cmath>
#include <cstdint>
#include <vector>
#include <algorithm>

#include "nengine/render/matrix.hpp"
#include "nengine/render/render_snapshot.hpp"

namespace nengine::render {

// REND-011: stable order for equal layers, based on World entity identity.
// This is testable without a GPU; final on-screen alpha/depth ordering is
// separately subject to real Vulkan validation.
inline void sort_sprites_for_draw(std::vector<SpriteItem>& sprites) {
    std::stable_sort(sprites.begin(),sprites.end(),[](const auto& a,const auto& b){
        if(a.renderer.sort_order!=b.renderer.sort_order)
            return a.renderer.sort_order<b.renderer.sort_order;
        return a.entity.value<b.entity.value;
    });
}

// The builtin quad is [-0.5,+0.5] in XY. pivot_x/y are normalized 0..1.
// Default (0.5,0.5) retains the original centered sprite placement.
inline Mat4 sprite_model_matrix(
    const SpriteItem& item,float width_units,float height_units) noexcept {
    if(!std::isfinite(width_units)||!std::isfinite(height_units) ||
       width_units<=0.f||height_units<=0.f)return item.world;
    const float px=std::clamp(item.renderer.pivot.x,0.f,1.f);
    const float py=std::clamp(item.renderer.pivot.y,0.f,1.f);
    Mat4 offset=Mat4::identity();
    offset.at(0,3)=(0.5f-px)*width_units;
    offset.at(1,3)=(0.5f-py)*height_units;
    const auto quad_scale=scaling_matrix({
        item.renderer.flip_x?-width_units:width_units,
        item.renderer.flip_y?-height_units:height_units,1.f
    });
    return multiply(item.world,multiply(offset,quad_scale));
}

} // namespace nengine::render

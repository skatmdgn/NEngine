#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include "nengine/core/world.hpp"
#include "nengine/render/tilemap.hpp"

namespace nengine::render {

// Runtime bridge: turns contiguous filled cells into static physics 2D boxes.
// Stores full generational Entity handles so unrelated entities never unload.
class TilemapCollisionLayer {
public:
    bool rebuild(core::World& world, const Tilemap& map,
                 core::Vec3 origin, std::uint32_t layer = 0,
                 bool trigger = false, std::string* error = nullptr);
    bool clear(core::World& world, std::string* error = nullptr);
    std::size_t collider_count() const noexcept { return owned_.size(); }
private:
    std::vector<core::Entity> owned_{};
};
} // namespace nengine::render

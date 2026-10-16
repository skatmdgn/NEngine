#include "nengine/player/managed_queries.hpp"
#include "nengine/scripting/managed_runtime.hpp"
#include "nengine/physics/collision.hpp"
#include <algorithm>
#include <optional>
#include <vector>

namespace nengine::player {
void bind_player_physics_queries(scripting::ManagedRuntime& runtime) {
    runtime.bind_physics_queries(nullptr,
        [](void*, const core::World& world, bool two_d,
           core::Vec3 origin, core::Vec3 direction, float maximum,
           bool include_triggers, std::uint32_t layers,
           core::Entity& entity, core::Vec3& point, core::Vec3& normal,
           float& distance, bool& trigger) {
            const auto hit = two_d
                ? physics::raycast_2d(world, {origin.x, origin.y},
                      {direction.x, direction.y}, maximum,
                      include_triggers, layers)
                : physics::raycast(world, origin, direction,
                      maximum, include_triggers, layers);
            if (!hit) return false;
            entity = hit->entity; point = hit->point;
            normal = hit->normal; distance = hit->distance;
            trigger = hit->is_trigger;
            return true;
        },
        [](void*, const core::World& world, bool two_d,
           core::Vec3 center, core::Vec3 size, bool include_triggers,
           std::uint32_t layers, std::uint64_t* output,
           std::size_t capacity) -> std::size_t {
            const auto hits = two_d
                ? physics::overlap_box_2d(world, {center.x, center.y},
                      {size.x, size.y}, include_triggers, layers)
                : physics::overlap_box(world, center, size,
                      include_triggers, layers);
            const auto count = std::min(hits.size(), capacity);
            if (output)
                for (std::size_t i = 0; i < count; ++i)
                    output[i] = hits[i].value;
            return hits.size();
        },
        [](void*, const core::World& world, bool two_d,
           core::Vec3 origin, core::Vec3 size, core::Vec3 direction,
           float maximum, bool include_triggers, std::uint32_t layers,
           core::Entity& entity, core::Vec3& point, core::Vec3& normal,
           float& distance, bool& trigger) {
            const auto hit = two_d
                ? physics::box_cast_2d(world, {origin.x, origin.y},
                      {size.x, size.y}, {direction.x, direction.y},
                      maximum, include_triggers, layers)
                : physics::box_cast(world, origin, size, direction,
                      maximum, include_triggers, layers);
            if (!hit) return false;
            entity = hit->entity; point = hit->point;
            normal = hit->normal; distance = hit->distance;
            trigger = hit->is_trigger;
            return true;
        });
}
}

#pragma once

#include <cstddef>

#include "nengine/core/math.hpp"
#include "nengine/core/world.hpp"

namespace nengine::physics {

struct PhysicsStepStats {
    std::size_t integrated_3d{0};
    std::size_t integrated_2d{0};
    std::size_t gravity_applied{0};
};

PhysicsStepStats step_rigidbodies(
    core::World& world,
    float delta_seconds,
    core::Vec3 gravity =
        {0.0f, -9.81f, 0.0f}) noexcept;

} // namespace nengine::physics

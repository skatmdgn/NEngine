#pragma once

#include "nengine/core/entity.hpp"
#include "nengine/core/math.hpp"

namespace nengine::core {

struct Transform {
    Vec3 local_position{};
    Quat local_rotation{};
    Vec3 local_scale{1.0f, 1.0f, 1.0f};
    Entity parent{Entity::invalid()};
};

} // namespace nengine::core

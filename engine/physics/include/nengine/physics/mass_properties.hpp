#pragma once

#include <algorithm>
#include <cmath>

namespace nengine::physics {

// Uniform-density capsule = a cylinder plus two solid hemispheres.
// The mass is divided by geometric volume rather than by height.
struct CapsuleInertia3D {
    float axial{0.0f};
    float transverse{0.0f};
};

inline CapsuleInertia3D capsule_inertia_3d(
    float mass, float radius, float cylinder_length) noexcept {

    if (!std::isfinite(mass) || !std::isfinite(radius) ||
        !std::isfinite(cylinder_length) || mass <= 0.0f ||
        radius <= 0.0f)
        return {};

    const float length = std::max(0.0f, cylinder_length);
    // pi cancels from the mass partition.
    const float cylinder_volume = radius * radius * length;
    const float cap_volume = (4.0f / 3.0f) * radius * radius * radius;
    const float total = cylinder_volume + cap_volume;
    const float cylinder_mass = mass * (cylinder_volume / total);
    const float cap_mass = mass - cylinder_mass;
    const float radius2 = radius * radius;
    const float length2 = length * length;

    // The hemisphere terms include each cap's centroid offset.
    return {
        0.5f * cylinder_mass * radius2 +
            0.4f * cap_mass * radius2,
        cylinder_mass * (3.0f * radius2 + length2) / 12.0f +
            cap_mass * (0.4f * radius2 +
                        0.375f * length * radius +
                        0.25f * length2)
    };
}

} // namespace nengine::physics

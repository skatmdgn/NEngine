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

// Uniform-density 2D stadium = a rectangle plus two semicircles.
// The parallel-axis contribution of the semicircles is integrated
// around their original circle centers.
inline float capsule_inertia_2d(
    float mass, float radius, float straight_length) noexcept {

    if (!std::isfinite(mass) || !std::isfinite(radius) ||
        !std::isfinite(straight_length) || mass <= 0.0f ||
        radius <= 0.0f)
        return 0.0f;

    const float length = std::max(0.0f, straight_length);
    constexpr float pi = 3.14159265358979323846f;
    const float rectangle_area = 2.0f * radius * length;
    const float caps_area = pi * radius * radius;
    const float total_area = rectangle_area + caps_area;
    const float rectangle_mass = mass * (rectangle_area / total_area);
    const float caps_mass = mass - rectangle_mass;
    const float r2 = radius * radius;

    return rectangle_mass * (4.0f * r2 + length * length) / 12.0f +
        caps_mass * (0.5f * r2 +
            (4.0f / (3.0f * pi)) * length * radius +
            0.25f * length * length);
}

} // namespace nengine::physics

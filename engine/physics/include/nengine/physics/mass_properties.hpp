#pragma once

#include <algorithm>
#include <array>

#include "nengine/core/math.hpp"
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

// Each element carries its geometric volume, center relative to the
// Rigidbody Transform pivot, and local inertia *per unit of mass*.
// Off-diagonal products are not yet represented by the solver's tensor.
struct MassElement3D {
    float measure{0.0f};
    core::Vec3 center{};
    core::Vec3 unit_inertia{};
};

inline core::Vec3 compound_inertia_3d(
    float mass, const std::array<MassElement3D, 3>& elements) noexcept {

    if (!std::isfinite(mass) || mass <= 0.0f) return {};
    float total = 0.0f;
    for (const auto& part : elements)
        if (std::isfinite(part.measure) && part.measure > 0.0f)
            total += part.measure;
    if (!std::isfinite(total) || total <= 0.0f) return {};

    core::Vec3 result{};
    for (const auto& part : elements) {
        if (!(part.measure > 0.0f) || !std::isfinite(part.measure))
            continue;
        const auto& p = part.center;
        const auto& i = part.unit_inertia;
        if (!std::isfinite(p.x) || !std::isfinite(p.y) ||
            !std::isfinite(p.z) || !std::isfinite(i.x) ||
            !std::isfinite(i.y) || !std::isfinite(i.z))
            continue;
        const float portion = mass * part.measure / total;
        result.x += portion * (i.x + p.y * p.y + p.z * p.z);
        result.y += portion * (i.y + p.x * p.x + p.z * p.z);
        result.z += portion * (i.z + p.x * p.x + p.y * p.y);
    }
    return result;
}

} // namespace nengine::physics

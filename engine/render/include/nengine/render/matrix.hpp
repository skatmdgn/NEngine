#pragma once

#include <array>
#include <optional>

#include "nengine/core/math.hpp"
#include "nengine/core/transform.hpp"
#include "nengine/core/world.hpp"
#include "nengine/render/components.hpp"

namespace nengine::render {

struct Mat4 {
    std::array<float, 16> value{};

    static Mat4 identity() noexcept;

    float& at(
        int row,
        int column) noexcept {
        return value[
            static_cast<std::size_t>(
                column * 4 + row)];
    }

    float at(
        int row,
        int column) const noexcept {
        return value[
            static_cast<std::size_t>(
                column * 4 + row)];
    }

    friend bool operator==(
        const Mat4&,
        const Mat4&) = default;
};

Mat4 multiply(
    const Mat4& a,
    const Mat4& b) noexcept;

Mat4 scaling_matrix(
    core::Vec3 value) noexcept;

core::Vec3 transform_point(
    const Mat4& matrix,
    core::Vec3 point) noexcept;

Mat4 transform_matrix(
    const core::Transform& transform) noexcept;

std::optional<Mat4> inverse_affine(
    const Mat4& matrix) noexcept;

Mat4 world_matrix(
    const core::World& world,
    core::Entity entity) noexcept;

Mat4 perspective_lh_zo(
    float vertical_fov_degrees,
    float aspect,
    float near_clip,
    float far_clip) noexcept;

Mat4 orthographic_lh_zo(
    float half_height,
    float aspect,
    float near_clip,
    float far_clip) noexcept;

struct CameraMatrices {
    Mat4 world{};
    Mat4 view{};
    Mat4 projection{};
    Mat4 view_projection{};
};

std::optional<CameraMatrices>
build_camera_matrices(
    const core::World& world,
    core::Entity entity,
    float aspect) noexcept;

} // namespace nengine::render

#include "nengine/render/matrix.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_set>
#include <vector>

namespace nengine::render {
namespace {

constexpr float pi =
    3.14159265358979323846f;

core::Quat normalized(
    core::Quat q) noexcept {

    const float length_squared =
        q.x * q.x +
        q.y * q.y +
        q.z * q.z +
        q.w * q.w;

    if (length_squared <=
        1.0e-12f) {
        return {};
    }

    const float inverse_length =
        1.0f /
        std::sqrt(
            length_squared);

    q.x *= inverse_length;
    q.y *= inverse_length;
    q.z *= inverse_length;
    q.w *= inverse_length;

    return q;
}

Mat4 translation_matrix(
    core::Vec3 value) noexcept {

    auto result =
        Mat4::identity();

    result.at(0, 3) = value.x;
    result.at(1, 3) = value.y;
    result.at(2, 3) = value.z;

    return result;
}

Mat4 scale_matrix(
    core::Vec3 value) noexcept {

    Mat4 result{};
    result.at(0, 0) = value.x;
    result.at(1, 1) = value.y;
    result.at(2, 2) = value.z;
    result.at(3, 3) = 1.0f;
    return result;
}

Mat4 rotation_matrix(
    core::Quat value) noexcept {

    const auto q =
        normalized(value);

    const float xx = q.x * q.x;
    const float yy = q.y * q.y;
    const float zz = q.z * q.z;
    const float xy = q.x * q.y;
    const float xz = q.x * q.z;
    const float yz = q.y * q.z;
    const float wx = q.w * q.x;
    const float wy = q.w * q.y;
    const float wz = q.w * q.z;

    auto result =
        Mat4::identity();

    result.at(0, 0) =
        1.0f -
        2.0f * (yy + zz);

    result.at(0, 1) =
        2.0f * (xy - wz);

    result.at(0, 2) =
        2.0f * (xz + wy);

    result.at(1, 0) =
        2.0f * (xy + wz);

    result.at(1, 1) =
        1.0f -
        2.0f * (xx + zz);

    result.at(1, 2) =
        2.0f * (yz - wx);

    result.at(2, 0) =
        2.0f * (xz - wy);

    result.at(2, 1) =
        2.0f * (yz + wx);

    result.at(2, 2) =
        1.0f -
        2.0f * (xx + yy);

    return result;
}

} // namespace

Mat4 scaling_matrix(
    core::Vec3 value) noexcept {

    return scale_matrix(
        value);
}

Mat4 Mat4::identity() noexcept {
    Mat4 result{};
    result.at(0, 0) = 1.0f;
    result.at(1, 1) = 1.0f;
    result.at(2, 2) = 1.0f;
    result.at(3, 3) = 1.0f;
    return result;
}

Mat4 multiply(
    const Mat4& a,
    const Mat4& b) noexcept {

    Mat4 result{};

    for (int column = 0;
         column < 4;
         ++column) {

        for (int row = 0;
             row < 4;
             ++row) {

            float value = 0.0f;

            for (int k = 0;
                 k < 4;
                 ++k) {

                value +=
                    a.at(row, k) *
                    b.at(k, column);
            }

            result.at(
                row,
                column) = value;
        }
    }

    return result;
}

core::Vec3 transform_point(
    const Mat4& matrix,
    core::Vec3 point) noexcept {

    const float x =
        matrix.at(0, 0) * point.x +
        matrix.at(0, 1) * point.y +
        matrix.at(0, 2) * point.z +
        matrix.at(0, 3);

    const float y =
        matrix.at(1, 0) * point.x +
        matrix.at(1, 1) * point.y +
        matrix.at(1, 2) * point.z +
        matrix.at(1, 3);

    const float z =
        matrix.at(2, 0) * point.x +
        matrix.at(2, 1) * point.y +
        matrix.at(2, 2) * point.z +
        matrix.at(2, 3);

    const float w =
        matrix.at(3, 0) * point.x +
        matrix.at(3, 1) * point.y +
        matrix.at(3, 2) * point.z +
        matrix.at(3, 3);

    if (std::fabs(w) >
        1.0e-8f &&
        std::fabs(w - 1.0f) >
            1.0e-8f) {

        return {
            x / w,
            y / w,
            z / w
        };
    }

    return {x, y, z};
}

Mat4 transform_matrix(
    const core::Transform& transform) noexcept {

    return multiply(
        translation_matrix(
            transform.local_position),
        multiply(
            rotation_matrix(
                transform.local_rotation),
            scale_matrix(
                transform.local_scale)));
}

std::optional<Mat4> inverse_affine(
    const Mat4& matrix) noexcept {

    const float a00 =
        matrix.at(0, 0);

    const float a01 =
        matrix.at(0, 1);

    const float a02 =
        matrix.at(0, 2);

    const float a10 =
        matrix.at(1, 0);

    const float a11 =
        matrix.at(1, 1);

    const float a12 =
        matrix.at(1, 2);

    const float a20 =
        matrix.at(2, 0);

    const float a21 =
        matrix.at(2, 1);

    const float a22 =
        matrix.at(2, 2);

    const float determinant =
        a00 * (
            a11 * a22 -
            a12 * a21) -
        a01 * (
            a10 * a22 -
            a12 * a20) +
        a02 * (
            a10 * a21 -
            a11 * a20);

    if (std::fabs(determinant) <
        1.0e-8f) {
        return std::nullopt;
    }

    const float inverse_det =
        1.0f / determinant;

    Mat4 result =
        Mat4::identity();

    result.at(0, 0) =
        (a11 * a22 -
         a12 * a21) *
        inverse_det;

    result.at(0, 1) =
        (a02 * a21 -
         a01 * a22) *
        inverse_det;

    result.at(0, 2) =
        (a01 * a12 -
         a02 * a11) *
        inverse_det;

    result.at(1, 0) =
        (a12 * a20 -
         a10 * a22) *
        inverse_det;

    result.at(1, 1) =
        (a00 * a22 -
         a02 * a20) *
        inverse_det;

    result.at(1, 2) =
        (a02 * a10 -
         a00 * a12) *
        inverse_det;

    result.at(2, 0) =
        (a10 * a21 -
         a11 * a20) *
        inverse_det;

    result.at(2, 1) =
        (a01 * a20 -
         a00 * a21) *
        inverse_det;

    result.at(2, 2) =
        (a00 * a11 -
         a01 * a10) *
        inverse_det;

    const core::Vec3 translation{
        matrix.at(0, 3),
        matrix.at(1, 3),
        matrix.at(2, 3)
    };

    result.at(0, 3) =
        -(
            result.at(0, 0) *
                translation.x +
            result.at(0, 1) *
                translation.y +
            result.at(0, 2) *
                translation.z);

    result.at(1, 3) =
        -(
            result.at(1, 0) *
                translation.x +
            result.at(1, 1) *
                translation.y +
            result.at(1, 2) *
                translation.z);

    result.at(2, 3) =
        -(
            result.at(2, 0) *
                translation.x +
            result.at(2, 1) *
                translation.y +
            result.at(2, 2) *
                translation.z);

    return result;
}

Mat4 world_matrix(
    const core::World& world,
    core::Entity entity) noexcept {

    if (!world.is_alive(entity)) {
        return Mat4::identity();
    }

    std::vector<core::Entity> chain;
    std::unordered_set<
        core::Entity::value_type> visited;

    auto current = entity;

    while (world.is_alive(current) &&
           visited.insert(
               current.value).second) {

        chain.push_back(current);

        const auto* transform =
            world.transform(current);

        if (!transform ||
            !transform->parent.valid()) {
            break;
        }

        current =
            transform->parent;
    }

    Mat4 result =
        Mat4::identity();

    for (auto it =
             chain.rbegin();
         it != chain.rend();
         ++it) {

        const auto* transform =
            world.transform(*it);

        if (!transform) {
            continue;
        }

        result =
            multiply(
                result,
                transform_matrix(
                    *transform));
    }

    return result;
}

Mat4 perspective_lh_zo(
    float vertical_fov_degrees,
    float aspect,
    float near_clip,
    float far_clip) noexcept {

    vertical_fov_degrees =
        std::clamp(
            vertical_fov_degrees,
            1.0f,
            179.0f);

    aspect =
        std::max(
            aspect,
            1.0e-4f);

    near_clip =
        std::max(
            near_clip,
            1.0e-4f);

    far_clip =
        std::max(
            far_clip,
            near_clip + 1.0e-3f);

    const float f =
        1.0f /
        std::tan(
            vertical_fov_degrees *
            pi /
            360.0f);

    Mat4 result{};

    result.at(0, 0) =
        f / aspect;

    // Flip Y for Vulkan framebuffer coordinates.
    result.at(1, 1) =
        -f;

    result.at(2, 2) =
        far_clip /
        (far_clip -
         near_clip);

    result.at(2, 3) =
        -near_clip *
        far_clip /
        (far_clip -
         near_clip);

    result.at(3, 2) =
        1.0f;

    return result;
}

Mat4 orthographic_lh_zo(
    float half_height,
    float aspect,
    float near_clip,
    float far_clip) noexcept {

    half_height =
        std::max(
            half_height,
            1.0e-4f);

    aspect =
        std::max(
            aspect,
            1.0e-4f);

    far_clip =
        std::max(
            far_clip,
            near_clip + 1.0e-3f);

    const float half_width =
        half_height *
        aspect;

    auto result =
        Mat4::identity();

    result.at(0, 0) =
        1.0f /
        half_width;

    result.at(1, 1) =
        -1.0f /
        half_height;

    result.at(2, 2) =
        1.0f /
        (far_clip -
         near_clip);

    result.at(2, 3) =
        -near_clip /
        (far_clip -
         near_clip);

    return result;
}

std::optional<CameraMatrices>
build_camera_matrices(
    const core::World& world,
    core::Entity entity,
    float aspect) noexcept {

    const auto* camera =
        world.get_component<Camera>(
            entity,
            camera_type());

    if (!camera ||
        !camera->enabled ||
        !world.active(entity)) {
        return std::nullopt;
    }

    CameraMatrices result;

    result.world =
        world_matrix(
            world,
            entity);

    const auto inverse =
        inverse_affine(
            result.world);

    if (!inverse) {
        return std::nullopt;
    }

    result.view =
        *inverse;

    if (camera->projection ==
        ProjectionMode::Perspective) {

        result.projection =
            perspective_lh_zo(
                camera
                    ->vertical_fov_degrees,
                aspect,
                camera->near_clip,
                camera->far_clip);

    } else {
        result.projection =
            orthographic_lh_zo(
                camera
                    ->orthographic_size,
                aspect,
                camera->near_clip,
                camera->far_clip);
    }

    result.view_projection =
        multiply(
            result.projection,
            result.view);

    return result;
}

} // namespace nengine::render

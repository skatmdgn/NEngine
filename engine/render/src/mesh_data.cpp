#include "nengine/render/mesh_data.hpp"

#include <array>

namespace nengine::render {
namespace {

void append_face(
    MeshData& mesh,
    core::Vec3 a,
    core::Vec3 b,
    core::Vec3 c,
    core::Vec3 d,
    core::Vec3 normal) {

    const auto base =
        static_cast<std::uint32_t>(
            mesh.vertices.size());

    mesh.vertices.push_back({
        a,
        normal,
        {0.0f, 0.0f}
    });

    mesh.vertices.push_back({
        b,
        normal,
        {1.0f, 0.0f}
    });

    mesh.vertices.push_back({
        c,
        normal,
        {1.0f, 1.0f}
    });

    mesh.vertices.push_back({
        d,
        normal,
        {0.0f, 1.0f}
    });

    mesh.indices.insert(
        mesh.indices.end(),
        {
            base + 0u,
            base + 1u,
            base + 2u,
            base + 0u,
            base + 2u,
            base + 3u
        });
}

} // namespace

MeshData make_unit_cube_mesh() {
    MeshData mesh;

    mesh.vertices.reserve(24);
    mesh.indices.reserve(36);

    constexpr float h = 0.5f;

    append_face(
        mesh,
        {-h, -h,  h},
        { h, -h,  h},
        { h,  h,  h},
        {-h,  h,  h},
        {0.0f, 0.0f, 1.0f});

    append_face(
        mesh,
        { h, -h, -h},
        {-h, -h, -h},
        {-h,  h, -h},
        { h,  h, -h},
        {0.0f, 0.0f, -1.0f});

    append_face(
        mesh,
        {-h, -h, -h},
        {-h, -h,  h},
        {-h,  h,  h},
        {-h,  h, -h},
        {-1.0f, 0.0f, 0.0f});

    append_face(
        mesh,
        { h, -h,  h},
        { h, -h, -h},
        { h,  h, -h},
        { h,  h,  h},
        {1.0f, 0.0f, 0.0f});

    append_face(
        mesh,
        {-h,  h,  h},
        { h,  h,  h},
        { h,  h, -h},
        {-h,  h, -h},
        {0.0f, 1.0f, 0.0f});

    append_face(
        mesh,
        {-h, -h, -h},
        { h, -h, -h},
        { h, -h,  h},
        {-h, -h,  h},
        {0.0f, -1.0f, 0.0f});

    mesh.bounds.center =
        {0.0f, 0.0f, 0.0f};

    mesh.bounds.extents =
        {h, h, h};

    return mesh;
}

MeshData make_unit_quad_mesh() {
    MeshData mesh;

    mesh.vertices = {
        {
            {-0.5f, -0.5f, 0.0f},
            {0.0f, 0.0f, 1.0f},
            {0.0f, 0.0f}
        },
        {
            {0.5f, -0.5f, 0.0f},
            {0.0f, 0.0f, 1.0f},
            {1.0f, 0.0f}
        },
        {
            {0.5f, 0.5f, 0.0f},
            {0.0f, 0.0f, 1.0f},
            {1.0f, 1.0f}
        },
        {
            {-0.5f, 0.5f, 0.0f},
            {0.0f, 0.0f, 1.0f},
            {0.0f, 1.0f}
        }
    };

    mesh.indices = {
        0u, 1u, 2u,
        0u, 2u, 3u
    };

    mesh.bounds.center =
        {0.0f, 0.0f, 0.0f};

    mesh.bounds.extents =
        {0.5f, 0.5f, 0.0f};

    return mesh;
}

} // namespace nengine::render

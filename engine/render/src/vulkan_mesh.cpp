#include "nengine/render/vulkan_mesh.hpp"

#include <limits>

namespace nengine::render {

bool VulkanMeshResource::create(
    const VulkanLoader& loader,
    const VulkanInstance& instance,
    const VulkanDevice& device,
    const MeshData& mesh) {

    destroy();
    diagnostic_.clear();

    if (!mesh.valid()) {
        diagnostic_ =
            "valid CPU MeshData is required";
        return false;
    }

    if (mesh.vertices.size() >
            std::numeric_limits<
                std::uint32_t>::max() ||
        mesh.indices.size() >
            std::numeric_limits<
                std::uint32_t>::max()) {

        diagnostic_ =
            "mesh exceeds VulkanMeshResource indexable range";
        return false;
    }

    const auto vertex_bytes =
        mesh.vertices.size() *
        sizeof(MeshVertex);

    const auto index_bytes =
        mesh.indices.size() *
        sizeof(std::uint32_t);

    if (!vertex_buffer_.create(
            loader,
            instance,
            device,
            vertex_bytes,
            VulkanBufferUsage::Vertex,
            VulkanMemoryPreference::DeviceLocal,
            mesh.vertices.data())) {

        diagnostic_ =
            "vertex buffer creation failed: " +
            vertex_buffer_.diagnostic();

        destroy();
        return false;
    }

    if (!index_buffer_.create(
            loader,
            instance,
            device,
            index_bytes,
            VulkanBufferUsage::Index,
            VulkanMemoryPreference::DeviceLocal,
            mesh.indices.data())) {

        diagnostic_ =
            "index buffer creation failed: " +
            index_buffer_.diagnostic();

        destroy();
        return false;
    }

    index_count_ =
        static_cast<std::uint32_t>(
            mesh.indices.size());

    submeshes_ =
        mesh.submeshes;

    if (submeshes_.empty()) {
        submeshes_.push_back({
            0u,
            index_count_,
            kMeshMaterialUnassigned
        });
    }

    bounds_ =
        mesh.bounds;

    diagnostic_ =
        "Vulkan mesh resource created";

    return true;
}

void VulkanMeshResource::destroy() noexcept {
    index_buffer_.destroy();
    vertex_buffer_.destroy();
    index_count_ = 0;
    submeshes_.clear();
    bounds_ = {};
}

} // namespace nengine::render

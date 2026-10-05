#pragma once

#include <cstdint>
#include <string>

#include "nengine/render/mesh_data.hpp"
#include "nengine/render/vulkan_buffer.hpp"

namespace nengine::render {

class VulkanMeshResource {
public:
    VulkanMeshResource() = default;

    bool create(
        const VulkanLoader& loader,
        const VulkanInstance& instance,
        const VulkanDevice& device,
        const MeshData& mesh);

    void destroy() noexcept;

    bool valid() const noexcept {
        return vertex_buffer_.valid() &&
            index_buffer_.valid() &&
            index_count_ != 0;
    }

    const VulkanBufferResource&
    vertex_buffer() const noexcept {
        return vertex_buffer_;
    }

    const VulkanBufferResource&
    index_buffer() const noexcept {
        return index_buffer_;
    }

    std::uint32_t index_count() const noexcept {
        return index_count_;
    }

    const MeshBounds& bounds() const noexcept {
        return bounds_;
    }

    const std::string& diagnostic() const noexcept {
        return diagnostic_;
    }

private:
    VulkanBufferResource vertex_buffer_{};
    VulkanBufferResource index_buffer_{};
    std::uint32_t index_count_{0};
    MeshBounds bounds_{};
    std::string diagnostic_{};
};

} // namespace nengine::render

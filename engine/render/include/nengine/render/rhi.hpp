#pragma once

#include <cstddef>
#include <cstdint>

namespace nengine::render {

enum class GraphicsBackend : std::uint8_t {
    Vulkan,
};

struct BufferHandle {
    std::uint32_t index{0xFFFFFFFFu};
    std::uint32_t generation{0};
    constexpr bool valid() const noexcept {
        return index != 0xFFFFFFFFu;
    }
    friend constexpr bool operator==(BufferHandle, BufferHandle) = default;
};

struct TextureHandle {
    std::uint32_t index{0xFFFFFFFFu};
    std::uint32_t generation{0};
    constexpr bool valid() const noexcept {
        return index != 0xFFFFFFFFu;
    }
    friend constexpr bool operator==(TextureHandle, TextureHandle) = default;
};

struct PipelineHandle {
    std::uint32_t index{0xFFFFFFFFu};
    std::uint32_t generation{0};
    constexpr bool valid() const noexcept {
        return index != 0xFFFFFFFFu;
    }
    friend constexpr bool operator==(PipelineHandle, PipelineHandle) = default;
};

enum class BufferUsage : std::uint8_t {
    Vertex,
    Index,
    Uniform,
    Storage,
    Upload,
    Readback,
};

enum class TextureFormat : std::uint8_t {
    Unknown,
    RGBA8_UNorm,
    RGBA8_sRGB,
    BGRA8_sRGB,
    RGBA16_Float,
    D32_Float,
    D24S8,
};

enum class TextureUsage : std::uint8_t {
    Sampled,
    RenderTarget,
    DepthStencil,
    Storage,
};

enum class PrimitiveTopology : std::uint8_t {
    TriangleList,
    TriangleStrip,
    LineList,
    PointList,
};

enum class CullMode : std::uint8_t {
    None,
    Front,
    Back,
};

struct BufferDesc {
    std::size_t size_bytes{0};
    BufferUsage usage{BufferUsage::Vertex};
    bool cpu_visible{false};
};

struct TextureDesc {
    std::uint32_t width{1};
    std::uint32_t height{1};
    std::uint32_t depth{1};
    std::uint32_t mip_levels{1};
    std::uint32_t array_layers{1};
    TextureFormat format{TextureFormat::RGBA8_UNorm};
    TextureUsage usage{TextureUsage::Sampled};
};

struct PipelineDesc {
    PrimitiveTopology topology{PrimitiveTopology::TriangleList};
    CullMode cull_mode{CullMode::Back};
    bool depth_test{true};
    bool depth_write{true};
    TextureFormat color_format{TextureFormat::BGRA8_sRGB};
    TextureFormat depth_format{TextureFormat::D32_Float};
};

struct SwapchainDesc {
    std::uint32_t width{1};
    std::uint32_t height{1};
    std::uint32_t image_count{3};
    TextureFormat format{TextureFormat::BGRA8_sRGB};
    bool vsync{true};
};

class RenderDevice {
public:
    virtual ~RenderDevice() = default;

    virtual GraphicsBackend backend() const noexcept = 0;

    virtual BufferHandle create_buffer(
        const BufferDesc& desc,
        const void* initial_data = nullptr) = 0;

    virtual void destroy_buffer(BufferHandle handle) = 0;

    virtual TextureHandle create_texture(
        const TextureDesc& desc,
        const void* initial_data = nullptr,
        std::size_t initial_size = 0) = 0;

    virtual void destroy_texture(TextureHandle handle) = 0;

    virtual PipelineHandle create_pipeline(
        const PipelineDesc& desc) = 0;

    virtual void destroy_pipeline(PipelineHandle handle) = 0;

    virtual void wait_idle() = 0;
};

} // namespace nengine::render

#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "nengine/core/scene.hpp"

namespace nengine::core {

// Additive scene ownership is separate from Entity indexes/generations; a
// handle cannot be mistaken for another load even when a World slot is reused.
struct AdditiveHandle {
    std::uint64_t value{0};
    constexpr bool valid() const noexcept { return value != 0; }
    friend bool operator==(AdditiveHandle,AdditiveHandle)=default;
};

class AdditiveScenes {
public:
    // All changes commit together, including local EntityReference remapping.
    bool load(const SceneData& scene, World& world, AdditiveHandle& result,
              std::string* error=nullptr,
              const ComponentSerializationRegistry* codecs=nullptr);
    // Unload only entities still owned by this live handle. Cross-scene
    // references through registered component codecs are nulled transactionally.
    bool unload(AdditiveHandle handle, World& world,
                std::string* error=nullptr,
                const ComponentSerializationRegistry* codecs=nullptr);
    bool owns(AdditiveHandle handle, Entity entity) const noexcept;
    std::size_t count() const noexcept { return instances_.size(); }
    void clear() noexcept { instances_.clear(); }
private:
    struct Instance {
        AdditiveHandle handle{};
        std::vector<Entity> entities{};
    };
    std::vector<Instance> instances_{};
    std::uint64_t next_{1};
};
} // namespace nengine::core

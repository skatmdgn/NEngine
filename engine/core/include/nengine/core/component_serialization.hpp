#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "nengine/core/component_registry.hpp"
#include "nengine/core/property_value.hpp"
#include "nengine/core/world.hpp"

namespace nengine::core {

struct SerializedPropertyData {
    std::string name{};
    PropertyKind kind{PropertyKind::String};
    PropertyValue value{};
};

struct SerializedComponentData {
    ComponentTypeId type{
        ComponentRegistry::invalid_type};
    std::uint32_t version{1};
    std::string type_name{};
    std::vector<SerializedPropertyData> properties{};
};

class ComponentSerializationRegistry {
public:
    using Capture = std::function<
        std::optional<SerializedComponentData>(
            const World&,
            Entity)>;

    using Restore = std::function<
        bool(
            World&,
            Entity,
            const SerializedComponentData&,
            std::string*)>;

    struct Codec {
        ComponentTypeId type{
            ComponentRegistry::invalid_type};
        std::uint32_t version{1};
        std::string type_name{};
        Capture capture{};
        Restore restore{};
    };

    bool register_codec(Codec codec);

    const Codec* find(
        ComponentTypeId type) const noexcept;

    std::optional<SerializedComponentData> capture(
        const World& world,
        Entity entity,
        ComponentTypeId type) const;

    bool restore(
        World& world,
        Entity entity,
        const SerializedComponentData& data,
        std::string* error = nullptr) const;

private:
    std::unordered_map<ComponentTypeId, Codec> codecs_{};
};

} // namespace nengine::core

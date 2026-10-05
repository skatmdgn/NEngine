#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

#include "nengine/core/component_registry.hpp"
#include "nengine/core/property_value.hpp"
#include "nengine/core/world.hpp"

namespace nengine::editor {

class PropertyAccessRegistry {
public:
    using Getter = std::function<
        std::optional<core::PropertyValue>(
            const core::World&,
            core::Entity)>;

    using Setter = std::function<
        bool(
            core::World&,
            core::Entity,
            const core::PropertyValue&)>;

    struct Accessor {
        core::PropertyKind kind{core::PropertyKind::String};
        Getter getter{};
        Setter setter{};
    };

    bool register_property(
        core::ComponentTypeId component,
        std::string property_name,
        core::PropertyKind kind,
        Getter getter,
        Setter setter);

    const Accessor* find(
        core::ComponentTypeId component,
        std::string_view property_name) const noexcept;

    std::optional<core::PropertyValue> read(
        const core::World& world,
        core::Entity entity,
        core::ComponentTypeId component,
        std::string_view property_name) const;

    bool write(
        core::World& world,
        core::Entity entity,
        core::ComponentTypeId component,
        std::string_view property_name,
        const core::PropertyValue& value) const;

private:
    struct Key {
        core::ComponentTypeId component{};
        std::string property{};

        friend bool operator==(const Key&, const Key&) = default;
    };

    struct KeyHash {
        std::size_t operator()(const Key& key) const noexcept;
    };

    std::unordered_map<Key, Accessor, KeyHash> accessors_{};
};

} // namespace nengine::editor

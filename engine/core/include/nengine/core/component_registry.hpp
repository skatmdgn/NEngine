#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace nengine::core {

using ComponentTypeId = std::uint64_t;

enum class PropertyKind : std::uint8_t {
    Boolean,
    Integer,
    UnsignedInteger,
    Float,
    String,
    Vec3,
    Quaternion,
    EntityReference,
    AssetReference,
};

enum class PropertyFlags : std::uint32_t {
    None = 0,
    Serializable = 1u << 0u,
    Editable = 1u << 1u,
    ReadOnly = 1u << 2u,
    Hidden = 1u << 3u,
};

constexpr PropertyFlags operator|(PropertyFlags a, PropertyFlags b) noexcept {
    return static_cast<PropertyFlags>(static_cast<std::uint32_t>(a) | static_cast<std::uint32_t>(b));
}

constexpr bool has_flag(PropertyFlags value, PropertyFlags flag) noexcept {
    return (static_cast<std::uint32_t>(value) & static_cast<std::uint32_t>(flag)) != 0;
}

struct PropertyDescriptor {
    std::string name{};
    PropertyKind kind{PropertyKind::String};
    PropertyFlags flags{PropertyFlags::Serializable | PropertyFlags::Editable};
};

struct ComponentDescriptor {
    ComponentTypeId id{};
    std::string name{};
    std::string category{};
    bool builtin{false};
    bool allow_multiple{false};
    std::vector<PropertyDescriptor> properties{};
};

class ComponentRegistry {
public:
    static constexpr ComponentTypeId invalid_type = 0;

    static ComponentTypeId stable_id(std::string_view name) noexcept;

    bool register_type(std::string name, std::string category = {}, bool builtin = false, bool allow_multiple = false);
    bool unregister_type(ComponentTypeId id);
    bool register_property(ComponentTypeId component, PropertyDescriptor property);

    const ComponentDescriptor* find(ComponentTypeId id) const noexcept;
    const ComponentDescriptor* find(std::string_view name) const noexcept;
    std::vector<ComponentDescriptor> descriptors() const;
    std::size_t size() const noexcept { return by_id_.size(); }

private:
    std::unordered_map<ComponentTypeId, ComponentDescriptor> by_id_{};
    std::unordered_map<std::string, ComponentTypeId> by_name_{};
};

} // namespace nengine::core

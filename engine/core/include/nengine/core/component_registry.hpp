#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace nengine::core {

using ComponentTypeId = std::uint64_t;

struct ComponentDescriptor {
    ComponentTypeId id{};
    std::string name{};
    std::string category{};
    bool builtin{false};
};

class ComponentRegistry {
public:
    static constexpr ComponentTypeId invalid_type = 0;

    static ComponentTypeId stable_id(std::string_view name) noexcept;

    bool register_type(std::string name, std::string category = {}, bool builtin = false);
    bool unregister_type(ComponentTypeId id);

    const ComponentDescriptor* find(ComponentTypeId id) const noexcept;
    const ComponentDescriptor* find(std::string_view name) const noexcept;
    std::vector<ComponentDescriptor> descriptors() const;
    std::size_t size() const noexcept { return by_id_.size(); }

private:
    std::unordered_map<ComponentTypeId, ComponentDescriptor> by_id_{};
    std::unordered_map<std::string, ComponentTypeId> by_name_{};
};

} // namespace nengine::core

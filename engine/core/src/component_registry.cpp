#include "nengine/core/component_registry.hpp"

#include <algorithm>
#include <utility>

namespace nengine::core {

ComponentTypeId ComponentRegistry::stable_id(std::string_view name) noexcept {
    constexpr ComponentTypeId offset = 14695981039346656037ull;
    constexpr ComponentTypeId prime = 1099511628211ull;
    ComponentTypeId hash = offset;
    for (const unsigned char ch : name) {
        hash ^= static_cast<ComponentTypeId>(ch);
        hash *= prime;
    }
    return hash == invalid_type ? 1 : hash;
}

bool ComponentRegistry::register_type(std::string name, std::string category, bool builtin) {
    if (name.empty() || by_name_.contains(name)) {
        return false;
    }

    const auto id = stable_id(name);
    if (const auto it = by_id_.find(id); it != by_id_.end() && it->second.name != name) {
        return false;
    }

    ComponentDescriptor descriptor{id, std::move(name), std::move(category), builtin};
    by_name_.emplace(descriptor.name, descriptor.id);
    by_id_.emplace(descriptor.id, std::move(descriptor));
    return true;
}

bool ComponentRegistry::unregister_type(ComponentTypeId id) {
    const auto it = by_id_.find(id);
    if (it == by_id_.end()) {
        return false;
    }
    by_name_.erase(it->second.name);
    by_id_.erase(it);
    return true;
}

const ComponentDescriptor* ComponentRegistry::find(ComponentTypeId id) const noexcept {
    const auto it = by_id_.find(id);
    return it == by_id_.end() ? nullptr : &it->second;
}

const ComponentDescriptor* ComponentRegistry::find(std::string_view name) const noexcept {
    const auto it = by_name_.find(std::string{name});
    return it == by_name_.end() ? nullptr : find(it->second);
}

std::vector<ComponentDescriptor> ComponentRegistry::descriptors() const {
    std::vector<ComponentDescriptor> result;
    result.reserve(by_id_.size());
    for (const auto& [_, descriptor] : by_id_) {
        result.push_back(descriptor);
    }
    std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
        return a.name < b.name;
    });
    return result;
}

} // namespace nengine::core

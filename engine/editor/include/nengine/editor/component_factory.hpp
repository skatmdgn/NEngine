#pragma once

#include <functional>
#include <unordered_map>
#include <vector>

#include "nengine/core/component_registry.hpp"
#include "nengine/core/entity.hpp"
#include "nengine/core/world.hpp"

namespace nengine::editor {

class ComponentFactoryRegistry {
public:
    using Factory =
        std::function<
            bool(
                core::World&,
                core::Entity)>;

    bool register_factory(
        core::ComponentTypeId type,
        Factory factory);

    bool unregister_factory(
        core::ComponentTypeId type);

    bool add(
        core::World& world,
        core::Entity entity,
        core::ComponentTypeId type) const;

    bool contains(
        core::ComponentTypeId type) const noexcept;

    std::vector<core::ComponentTypeId>
    types() const;

private:
    std::unordered_map<
        core::ComponentTypeId,
        Factory> factories_{};
};

} // namespace nengine::editor

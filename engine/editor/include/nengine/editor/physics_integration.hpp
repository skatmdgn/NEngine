#pragma once

#include "nengine/core/component_registry.hpp"
#include "nengine/core/component_serialization.hpp"
#include "nengine/editor/component_factory.hpp"
#include "nengine/editor/property_access.hpp"

namespace nengine::editor {

bool register_physics_integration(
    core::ComponentRegistry& components,
    core::ComponentSerializationRegistry& serialization,
    PropertyAccessRegistry& properties);

bool register_physics_component_factories(
    ComponentFactoryRegistry& factories);

} // namespace nengine::editor

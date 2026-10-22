#pragma once
#include "nengine/core/component_registry.hpp"
#include "nengine/core/component_serialization.hpp"
#include "nengine/editor/component_factory.hpp"
#include "nengine/editor/property_access.hpp"
namespace nengine::editor {
bool register_navigation_integration(
 core::ComponentRegistry&,core::ComponentSerializationRegistry&,
 PropertyAccessRegistry&,ComponentFactoryRegistry&);
}

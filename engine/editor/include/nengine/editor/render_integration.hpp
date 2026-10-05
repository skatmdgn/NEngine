#pragma once

#include "nengine/core/component_registry.hpp"
#include "nengine/core/component_serialization.hpp"
#include "nengine/editor/property_access.hpp"

namespace nengine::editor {

bool register_render_integration(
    core::ComponentRegistry& components,
    core::ComponentSerializationRegistry& serialization,
    PropertyAccessRegistry& properties);

} // namespace nengine::editor

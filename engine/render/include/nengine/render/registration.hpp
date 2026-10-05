#pragma once

#include "nengine/core/component_registry.hpp"
#include "nengine/core/component_serialization.hpp"

namespace nengine::render {

bool register_component_metadata(
    core::ComponentRegistry& registry);

bool register_component_serializers(
    core::ComponentSerializationRegistry& registry);

} // namespace nengine::render

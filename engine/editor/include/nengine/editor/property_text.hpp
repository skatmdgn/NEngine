#pragma once

#include <string>
#include <string_view>

#include "nengine/core/component_registry.hpp"
#include "nengine/core/property_value.hpp"

namespace nengine::editor {

std::string format_property_value(
    const core::PropertyValue& value);

bool parse_property_value(
    core::PropertyKind kind,
    std::string_view text,
    core::PropertyValue& value,
    std::string* error = nullptr);

} // namespace nengine::editor

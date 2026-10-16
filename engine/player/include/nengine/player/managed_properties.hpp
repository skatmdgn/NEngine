#pragma once
#include <string_view>
#include "nengine/core/world.hpp"
#include "nengine/core/property_value.hpp"
namespace nengine::scripting { class ManagedRuntime; }
namespace nengine::player {
bool read_player_transform_property(const core::World& world,
    core::Entity entity, std::string_view component,
    std::string_view property, core::PropertyValue& value);
bool write_player_transform_property(core::World& world,
    core::Entity entity, std::string_view component,
    std::string_view property, const core::PropertyValue& value);
void bind_player_properties(scripting::ManagedRuntime& runtime);
}

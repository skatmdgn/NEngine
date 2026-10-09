#pragma once

#include <string>

#include "nengine/core/component_registry.hpp"

namespace nengine::scripting {

struct ScriptBehaviour {
    bool enabled{true};
    std::string type_name{};
};

inline core::ComponentTypeId
script_behaviour_type() noexcept {
    return core::ComponentRegistry::stable_id(
        "NEngine.ScriptBehaviour");
}

} // namespace nengine::scripting

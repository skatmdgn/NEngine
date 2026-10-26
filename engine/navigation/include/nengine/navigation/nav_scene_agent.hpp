#pragma once
#include <cstddef>
#include "nengine/core/world.hpp"
#include "nengine/navigation/components.hpp"
namespace nengine::navigation {
std::size_t step_scene_agents(core::World& world,const NavMesh& nav,
                              float delta_seconds);
}
#pragma once
namespace nengine::scripting { class ManagedRuntime; }
namespace nengine::player {
// Native query callbacks do not depend on the Editor model or its world.
void bind_player_physics_queries(scripting::ManagedRuntime& runtime);
}

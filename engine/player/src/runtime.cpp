#include "nengine/player/runtime.hpp"

#include "nengine/audio/registration.hpp"
#include "nengine/physics/registration.hpp"
#include "nengine/render/registration.hpp"
#include "nengine/scripting/registration.hpp"
#include "nengine/scripting/components.hpp"

namespace nengine::player {

std::size_t PlayerRuntime::pending_script_count() const noexcept {
    if (!ready_) return 0;
    std::size_t count = 0;
    for (const auto entity : world_.entities())
        if (world_.has_component(
                entity, scripting::script_behaviour_type()))
            ++count;
    return count;
}

void PlayerRuntime::unload() noexcept {
    world_.clear();
    ready_ = false;
}

bool PlayerRuntime::load(const std::filesystem::path& root,
                         std::string* error) {
    BundleManifest manifest;
    if (!load_bundle(root / "NEngine.player", manifest, error)) return false;
    const auto scene_path = resolve_bundle_scene(root, manifest, error);
    if (!scene_path) return false;
    core::SceneData source;
    if (!core::SceneSerializer::load_file(*scene_path, source, error))
        return false;

    core::ComponentSerializationRegistry codecs;
    if (!render::register_component_serializers(codecs) ||
        !physics::register_component_serializers(codecs) ||
        !audio::register_component_serializers(codecs) ||
        !scripting::register_component_serializers(codecs)) {
        if (error) *error = "player component codec registration failed";
        return false;
    }
    core::World next_world;
    if (!core::SceneSerializer::instantiate(
            source, next_world, error, &codecs))
        return false;
    world_ = std::move(next_world);
    manifest_ = std::move(manifest);
    ready_ = true;
    if (error) error->clear();
    return true;
}
} // namespace nengine::player

#include "nengine/player/runtime.hpp"

#include "nengine/audio/registration.hpp"
#include "nengine/physics/registration.hpp"
#include "nengine/render/registration.hpp"
#include "nengine/scripting/registration.hpp"
#include "nengine/scripting/components.hpp"
#include <algorithm>
#include <cmath>

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

PlayerFrameStats PlayerRuntime::advance(float delta_seconds) {
    PlayerFrameStats stats;
    if (!ready_ || !std::isfinite(delta_seconds) || delta_seconds <= 0.f)
        return stats;
    constexpr double fixed = 1.0 / 60.0;
    constexpr double max_frame = 0.25;
    const double frame = std::min(
        static_cast<double>(delta_seconds), max_frame);
    stats.clamped_delta = frame < static_cast<double>(delta_seconds);
    elapsed_seconds_ += frame;
    accumulator_ += frame;
    while (accumulator_ + 1.0e-9 >= fixed && stats.physics_steps < 8u) {
        const auto result = physics::step_physics(
            world_, static_cast<float>(fixed), {0, -9.81f, 0},
            &physics_cache_);
        ++stats.physics_steps;
        stats.integrated_3d += result.integration.integrated_3d;
        stats.integrated_2d += result.integration.integrated_2d;
        accumulator_ -= fixed;
    }
    // Avoid an unbounded backlog after a debugger stop or window stall.
    if (accumulator_ >= fixed) {
        stats.clamped_delta = true;
        accumulator_ = std::fmod(accumulator_, fixed);
    }
    return stats;
}

void PlayerRuntime::unload() noexcept {
    world_.clear();
    assets_.clear();
    physics_cache_.clear();
    accumulator_ = 0.0;
    elapsed_seconds_ = 0.0;
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
    PlayerAssets catalog;
    if (!catalog.open(root, error)) return false;
    world_ = std::move(next_world);
    assets_ = std::move(catalog);
    physics_cache_.clear();
    accumulator_ = 0.0;
    elapsed_seconds_ = 0.0;
    manifest_ = std::move(manifest);
    ready_ = true;
    if (error) error->clear();
    return true;
}
} // namespace nengine::player

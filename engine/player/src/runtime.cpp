#include "nengine/player/runtime.hpp"
#include "nengine/player/managed_queries.hpp"
#include "nengine/player/managed_properties.hpp"
#include "nengine/input/action_map_io.hpp"

#include "nengine/audio/registration.hpp"
#include "nengine/audio/components.hpp"
#include "nengine/render/components.hpp"
#include "nengine/physics/registration.hpp"
#include "nengine/render/registration.hpp"
#include "nengine/scripting/registration.hpp"
#include "nengine/scripting/components.hpp"
#include "nengine/scripting/dotnet_host.hpp"
#include <algorithm>
#include <cmath>

namespace nengine::player {

bool PlayerRuntime::initialize_managed(std::string* error) {
    if (!ready_ || bundle_root_.empty()) {
        if (error) *error = "Player Scene must load before .NET runtime";
        return false;
    }
    const auto managed = bundle_root_ / "Content" / "Managed";
    const auto gameplay = managed / "GameScripts.dll";
    const auto configuration = managed / "GameScripts.runtimeconfig.json";
    std::error_code ec;
    if (!std::filesystem::is_regular_file(gameplay, ec) ||
        !std::filesystem::is_regular_file(configuration, ec) ||
        !std::filesystem::is_regular_file(managed / "NEngine.Bridge.dll", ec) ||
        !std::filesystem::is_regular_file(managed / "NEngine.API.dll", ec)) {
        if (error) *error = "Player managed payload missing";
        return false;
    }
    std::string discovery_error;
    const auto host = scripting::discover_dotnet_host({}, &discovery_error);
    if (!host) {
        if (error) *error = std::move(discovery_error);
        return false;
    }
    script_system_.clear(&world_);
    managed_runtime_.shutdown();
    if (!managed_runtime_.initialize(
            host->hostfxr_path, configuration, gameplay, "GameScripts")) {
        if (error) *error = managed_runtime_.diagnostic();
        return false;
    }
    managed_runtime_.bind_input(&input_);
    managed_runtime_.bind_input_actions(&actions_);
    bind_player_physics_queries(managed_runtime_);
    bind_player_properties(managed_runtime_);
    script_system_.bind(&managed_runtime_);
    if (error) error->clear();
    return true;
}

const audio::AudioClipData* PlayerRuntime::audio_clip(
    assets::AssetGuid guid) const noexcept {
    const auto it = audio_clips_.find(guid);
    return it == audio_clips_.end() ? nullptr : &it->second;
}

std::size_t PlayerRuntime::pending_script_count() const noexcept {
    if (!ready_) return 0;
    std::size_t count = 0;
    for (const auto entity : world_.entities())
        if (world_.has_component(
                entity, scripting::script_behaviour_type()))
            ++count;
    return count;
}

void PlayerRuntime::ensure_referenced_assets() {
    // Bound negative caches against streams of unique invalid GUIDs from C#.
    const auto cache_miss = [](auto& cache, assets::AssetGuid guid) {
        constexpr std::size_t max_misses = 512;
        if (cache.size() >= max_misses) cache.clear();
        cache.insert(guid);
    };
    // Editor scene import preloads only the clips referenced at load time.
    // Scripts may assign a different, already bundled AssetGuid at runtime.
    for (const auto entity : world_.entities()) {
        if (const auto* audio = world_.get_component<audio::AudioSource>(
                entity, audio::audio_source_type())) {
            const auto guid = audio->clip;
            if (guid.valid() && !audio_clips_.contains(guid) &&
                !missing_audio_clips_.contains(guid)) {
                if (auto clip = assets_.load_wav(guid))
                    audio_clips_.emplace(guid, std::move(*clip));
                else
                    cache_miss(missing_audio_clips_, guid);
            }
        }
        if (const auto* animator = world_.get_component<render::SpriteAnimator>(
                entity, render::sprite_animator_type())) {
            const auto existing = controller_bindings_.find(entity.value);
            if (existing != controller_bindings_.end() &&
                existing->second != animator->controller) {
                sprite_controllers_.unbind(entity);
                controller_bindings_.erase(existing);
            }
            if (animator->controller.valid() &&
                !controller_bindings_.contains(entity.value) &&
                !missing_controller_guids_.contains(animator->controller)) {
                auto definition = assets_.load_controller(animator->controller);
                if (definition) {
                    // Controller states can reference clips absent from the
                    // startup scene. Prefetch before transition evaluation.
                    for (const auto& state : definition->states) {
                        if (!sprite_clips_.contains(state.clip) &&
                            !missing_sprite_clips_.contains(state.clip)) {
                            if (auto clip = assets_.load_sprite_clip(state.clip))
                                sprite_clips_.emplace(state.clip, std::move(*clip));
                            else cache_miss(missing_sprite_clips_, state.clip);
                        }
                    }
                }
                if (definition && sprite_controllers_.bind(entity, std::move(*definition)))
                    controller_bindings_[entity.value] = animator->controller;
                else
                    cache_miss(missing_controller_guids_, animator->controller);
            }
            const auto guid = animator->clip;
            if (guid.valid() && !sprite_clips_.contains(guid) &&
                !missing_sprite_clips_.contains(guid)) {
                if (auto clip = assets_.load_sprite_clip(guid))
                    sprite_clips_.emplace(guid, std::move(*clip));
                else
                    cache_miss(missing_sprite_clips_, guid);
            }
        }
    }
    // Removing/destroying an animator must also release its controller state.
    for (auto it = controller_bindings_.begin();
         it != controller_bindings_.end();) {
        const core::Entity entity{it->first};
        if (!world_.is_alive(entity) ||
            !world_.has_component(entity, render::sprite_animator_type())) {
            sprite_controllers_.unbind(entity);
            it = controller_bindings_.erase(it);
        } else ++it;
    }
}

PlayerFrameStats PlayerRuntime::advance(float delta_seconds) {
    PlayerFrameStats stats;
    if (!ready_ || paused_ ||
        !std::isfinite(delta_seconds) || delta_seconds <= 0.f)
        return stats;
    constexpr double fixed = 1.0 / 60.0;
    constexpr double max_frame = 0.25;
    const double frame = std::min(
        static_cast<double>(delta_seconds), max_frame);
    stats.clamped_delta = frame < static_cast<double>(delta_seconds);
    elapsed_seconds_ += frame;
    ensure_referenced_assets();
    for (const auto entity : world_.entities()) {
        const auto* animator = world_.get_component<render::SpriteAnimator>(
            entity, render::sprite_animator_type());
        if (!animator) continue;
        for (const auto& [name, value] : animator->bool_parameters)
            sprite_controllers_.set_bool(entity, name, value);
    }
    stats.controller_transitions =
        sprite_controllers_.evaluate(world_).transitions;
    // A controller can assign a different state clip in this same frame.
    ensure_referenced_assets();
    for (const auto entity : world_.entities()) {
        if (!world_.active(entity)) continue;
        auto* sprite = world_.get_component<render::SpriteAnimator>(
            entity, render::sprite_animator_type());
        if (!sprite || !sprite->enabled || !sprite->playing) continue;
        const auto it = sprite_clips_.find(sprite->clip);
        if (it == sprite_clips_.end()) {
            ++stats.sprite_unresolved;
            continue;
        }
        if (std::isfinite(sprite->speed) && sprite->speed > 0.f) {
            const double advanced =
                static_cast<double>(sprite->time_seconds) +
                frame * static_cast<double>(sprite->speed);
            const double duration = static_cast<double>(
                it->second.duration_seconds());
            if (std::isfinite(advanced) && duration > 0.0 &&
                std::isfinite(duration)) {
                // Keep looping/non-looping playback bounded across long sessions.
                const double stable = sprite->loop
                    ? std::fmod(std::max(0.0, advanced), duration)
                    : std::min(std::max(0.0, advanced), duration);
                float next_time = static_cast<float>(stable);
                // Float rounding can lift a double-precision remainder to
                // precisely duration, which is not in the looping interval.
                if (sprite->loop &&
                    next_time >= it->second.duration_seconds())
                    next_time = 0.f;
                sprite->time_seconds = next_time;
            }
        }
        const auto sampled = render::sample_sprite_animation_frame(
            it->second, sprite->time_seconds, sprite->loop);
        if (!sampled) { ++stats.sprite_unresolved; continue; }
        if (!sprite->loop && sprite->playing &&
            sprite->time_seconds >= it->second.duration_seconds()) {
            sprite->time_seconds = it->second.duration_seconds();
            sprite->playing = false;
        }
        if (auto* renderer = world_.get_component<render::SpriteRenderer>(
                entity, render::sprite_renderer_type());
            renderer && renderer->enabled) {
            renderer->texture = it->second.frames[*sampled].texture;
            ++stats.sprite_sampled;
        }
    }
    stats.audio = audio_playback_.update(world_, static_cast<float>(frame),
        [this](assets::AssetGuid guid) -> std::optional<float> {
            if (const auto* clip = audio_clip(guid))
                return clip->duration_seconds();
            return std::nullopt;
        });
    accumulator_ += frame;
    while (accumulator_ + 1.0e-9 >= fixed && stats.physics_steps < 8u) {
        if (managed_runtime_.valid()) {
            const auto script = script_system_.fixed_update(
                world_, static_cast<float>(fixed));
            stats.managed_fixed_updates += script.fixed_updated;
            stats.managed_errors += script.unresolved;
        }
        const auto result = physics::step_physics(
            world_, static_cast<float>(fixed), {0, -9.81f, 0},
            &physics_cache_);
        ++stats.physics_steps;
        stats.integrated_3d += result.integration.integrated_3d;
        stats.integrated_2d += result.integration.integrated_2d;
        stats.physics_contacts += result.collisions.overlaps.size();
        const auto contacts = contact_tracker_.update(result.collisions.overlaps);
        stats.contact_events += contacts.size();
        if (managed_runtime_.valid()) {
            for (const auto& event : contacts) {
                const int phase = static_cast<int>(event.phase);
                if (!script_system_.dispatch_physics_event(
                        world_, event.first, event.second, phase,
                        event.is_trigger, event.is_2d, event.normal,
                        event.penetration))
                    ++stats.managed_errors;
                if (!script_system_.dispatch_physics_event(
                        world_, event.second, event.first, phase,
                        event.is_trigger, event.is_2d,
                        {-event.normal.x, -event.normal.y, -event.normal.z},
                        event.penetration))
                    ++stats.managed_errors;
            }
        }
        accumulator_ -= fixed;
    }
    // Avoid an unbounded backlog after a debugger stop or window stall.
    if (accumulator_ >= fixed) {
        stats.clamped_delta = true;
        accumulator_ = std::fmod(accumulator_, fixed);
    }
    if (managed_runtime_.valid()) {
        const auto script = script_system_.update(
            world_, static_cast<float>(frame));
        stats.managed_updates += script.updated + script.late_updated;
        stats.managed_errors += script.unresolved;
    }
    return stats;
}

void PlayerRuntime::unload() noexcept {
    script_system_.clear(&world_);
    managed_runtime_.shutdown();
    world_.clear();
    input_.release_all();
    actions_ = {};
    bundle_root_.clear();
    assets_.clear();
    ui_.reset();
    audio_clips_.clear();
    sprite_clips_.clear();
    missing_audio_clips_.clear();
    missing_sprite_clips_.clear();
    sprite_controllers_.clear();
    controller_bindings_.clear();
    missing_controller_guids_.clear();
    audio_playback_.reset();
    physics_cache_.clear();
    contact_tracker_.clear();
    accumulator_ = 0.0;
    elapsed_seconds_ = 0.0;
    ready_ = false;
    paused_ = false;
}

bool PlayerRuntime::load(const std::filesystem::path& root,
                         std::string* error) {
    BundleManifest manifest;
    if (!load_bundle(root / "NEngine.player", manifest, error)) return false;
    const auto scene_path = resolve_bundle_scene(root, manifest, error);
    if (!scene_path) return false;
    input::ActionMap next_actions;
    const auto action_file = root / "Content" / "Input.ninput";
    std::error_code action_ec;
    const auto action_status =
        std::filesystem::symlink_status(action_file, action_ec);
    if (action_ec == std::errc::no_such_file_or_directory)
        action_ec.clear();
    if (action_ec || std::filesystem::is_symlink(action_status)) {
        if (error) *error = "Player action map path is unsafe";
        return false;
    }
    if (std::filesystem::exists(action_status)) {
        if (!input::load_action_map_file(action_file, next_actions, error))
            return false;
    }
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
    PlayerUiRuntime next_ui;
    if (!next_ui.load(root, error)) return false;
    std::unordered_map<assets::AssetGuid, audio::AudioClipData,
                       assets::AssetGuidHash> next_clips;
    for (const auto entity : next_world.entities()) {
        const auto* source = next_world.get_component<audio::AudioSource>(
            entity, audio::audio_source_type());
        if (!source || !source->clip.valid() ||
            next_clips.contains(source->clip)) continue;
        if (auto clip = catalog.load_wav(source->clip))
            next_clips.emplace(source->clip, std::move(*clip));
    }
    std::unordered_map<assets::AssetGuid, render::SpriteAnimationClip,
                       assets::AssetGuidHash> next_sprites;
    for (const auto entity : next_world.entities()) {
        const auto* animator = next_world.get_component<render::SpriteAnimator>(
            entity, render::sprite_animator_type());
        if (!animator || !animator->clip.valid() ||
            next_sprites.contains(animator->clip)) continue;
        if (auto clip = catalog.load_sprite_clip(animator->clip))
            next_sprites.emplace(animator->clip, std::move(*clip));
    }
    render::SpriteControllerRuntime next_controllers;
    std::unordered_map<std::uint64_t, assets::AssetGuid> next_bindings;
    for (const auto entity : next_world.entities()) {
        const auto* animator = next_world.get_component<render::SpriteAnimator>(
            entity, render::sprite_animator_type());
        if (!animator || !animator->controller.valid()) continue;
        auto definition = catalog.load_controller(animator->controller);
        if (!definition) continue;
        for (const auto& state : definition->states) {
            if (next_sprites.contains(state.clip)) continue;
            if (auto clip = catalog.load_sprite_clip(state.clip))
                next_sprites.emplace(state.clip, std::move(*clip));
        }
        if (next_controllers.bind(entity, std::move(*definition)))
            next_bindings.emplace(entity.value, animator->controller);
    }
    script_system_.clear(&world_);
    managed_runtime_.shutdown();
    world_ = std::move(next_world);
    bundle_root_ = root;
    input_ = {};
    actions_ = std::move(next_actions);
    assets_ = std::move(catalog);
    ui_ = std::move(next_ui);
    audio_clips_ = std::move(next_clips);
    sprite_clips_ = std::move(next_sprites);
    missing_audio_clips_.clear();
    missing_sprite_clips_.clear();
    sprite_controllers_ = std::move(next_controllers);
    controller_bindings_ = std::move(next_bindings);
    missing_controller_guids_.clear();
    audio_playback_.reset();
    physics_cache_.clear();
    contact_tracker_.clear();
    accumulator_ = 0.0;
    elapsed_seconds_ = 0.0;
    manifest_ = std::move(manifest);
    ready_ = true;
    paused_ = false;
    if (error) error->clear();
    return true;
}
} // namespace nengine::player

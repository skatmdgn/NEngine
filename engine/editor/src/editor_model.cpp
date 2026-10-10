#include "nengine/editor/editor_model.hpp"
#include "nengine/editor/audio_integration.hpp"
#include "nengine/editor/render_integration.hpp"
#include "nengine/editor/physics_integration.hpp"
#include "nengine/editor/scripting_integration.hpp"
#include "nengine/physics/simulation.hpp"

namespace nengine::editor {
namespace {

void bind_managed_property_access(
    scripting::ManagedRuntime& runtime,
    PropertyAccessRegistry& properties) {

    runtime.bind_property_access(
        &properties,
        [](
            void* context,
            const core::World& world,
            core::Entity entity,
            std::string_view component_name,
            std::string_view property_name,
            core::PropertyValue& output) {

            auto* registry =
                static_cast<
                    PropertyAccessRegistry*>(
                        context);

            if (!registry) {
                return false;
            }

            const auto value =
                registry->read(
                    world,
                    entity,
                    core::ComponentRegistry::stable_id(
                        component_name),
                    property_name);

            if (!value) {
                return false;
            }

            output = *value;
            return true;
        },
        [](
            void* context,
            core::World& world,
            core::Entity entity,
            std::string_view component_name,
            std::string_view property_name,
            const core::PropertyValue& value) {

            auto* registry =
                static_cast<
                    PropertyAccessRegistry*>(
                        context);

            return registry &&
                registry->write(
                    world,
                    entity,
                    core::ComponentRegistry::stable_id(
                        component_name),
                    property_name,
                    value);
        });
}

void bind_managed_physics_queries(
    scripting::ManagedRuntime& runtime) {

    runtime.bind_physics_queries(
        nullptr,
        [](
            void*,
            const core::World& world,
            bool is_2d,
            core::Vec3 origin,
            core::Vec3 direction,
            float max_distance,
            bool include_triggers,
            std::uint32_t layer_mask,
            core::Entity& hit_entity,
            core::Vec3& point,
            core::Vec3& normal,
            float& distance,
            bool& is_trigger) {

            std::optional<
                physics::RaycastHit>
                hit;

            if (is_2d) {
                hit =
                    physics::raycast_2d(
                        world,
                        {origin.x, origin.y},
                        {direction.x, direction.y},
                        max_distance,
                        include_triggers,
                        layer_mask);
            } else {
                hit =
                    physics::raycast(
                        world,
                        origin,
                        direction,
                        max_distance,
                        include_triggers,
                        layer_mask);
            }

            if (!hit) {
                return false;
            }

            hit_entity = hit->entity;
            point = hit->point;
            normal = hit->normal;
            distance = hit->distance;
            is_trigger = hit->is_trigger;
            return true;
        },
        [](
            void*,
            const core::World& world,
            bool is_2d,
            core::Vec3 center,
            core::Vec3 size,
            bool include_triggers,
            std::uint32_t layer_mask,
            std::uint64_t* output,
            std::size_t capacity) {

            std::vector<core::Entity>
                hits;

            if (is_2d) {
                hits =
                    physics::overlap_box_2d(
                        world,
                        {center.x, center.y},
                        {size.x, size.y},
                        include_triggers,
                        layer_mask);
            } else {
                hits =
                    physics::overlap_box(
                        world,
                        center,
                        size,
                        include_triggers,
                        layer_mask);
            }

            const auto count =
                std::min(
                    capacity,
                    hits.size());

            for (std::size_t index = 0;
                 index < count;
                 ++index) {
                output[index] =
                    hits[index].value;
            }

            return hits.size();
        },
        [](
            void*,
            const core::World& world,
            bool is_2d,
            core::Vec3 origin,
            core::Vec3 size,
            core::Vec3 direction,
            float max_distance,
            bool include_triggers,
            std::uint32_t layer_mask,
            core::Entity& hit_entity,
            core::Vec3& point,
            core::Vec3& normal,
            float& distance,
            bool& is_trigger) {

            std::optional<
                physics::RaycastHit>
                hit;

            if (is_2d) {
                hit =
                    physics::box_cast_2d(
                        world,
                        {origin.x, origin.y},
                        {size.x, size.y},
                        {direction.x, direction.y},
                        max_distance,
                        include_triggers,
                        layer_mask);
            } else {
                hit =
                    physics::box_cast(
                        world,
                        origin,
                        size,
                        direction,
                        max_distance,
                        include_triggers,
                        layer_mask);
            }

            if (!hit) {
                return false;
            }

            hit_entity = hit->entity;
            point = hit->point;
            normal = hit->normal;
            distance = hit->distance;
            is_trigger = hit->is_trigger;
            return true;
        });
}

} // namespace

EditorModel::EditorModel() {
    component_registry_.register_type(
        "NEngine.Transform",
        "Core",
        true,
        false);

    component_registry_.register_property(
        core::World::transform_type,
        {
            "Local Position",
            core::PropertyKind::Vec3,
            core::PropertyFlags::Serializable |
                core::PropertyFlags::Editable
        });

    component_registry_.register_property(
        core::World::transform_type,
        {
            "Local Rotation",
            core::PropertyKind::Quaternion,
            core::PropertyFlags::Serializable |
                core::PropertyFlags::Editable
        });

    component_registry_.register_property(
        core::World::transform_type,
        {
            "Local Scale",
            core::PropertyKind::Vec3,
            core::PropertyFlags::Serializable |
                core::PropertyFlags::Editable
        });

    property_access_.register_property(
        core::World::transform_type,
        "Local Position",
        core::PropertyKind::Vec3,
        [](const core::World& world, core::Entity entity)
            -> std::optional<core::PropertyValue> {
            const auto* transform = world.transform(entity);
            if (!transform) return std::nullopt;
            return core::PropertyValue{transform->local_position};
        },
        [](core::World& world,
           core::Entity entity,
           const core::PropertyValue& value) {
            auto* transform = world.transform(entity);
            const auto* typed = std::get_if<core::Vec3>(&value);
            if (!transform || !typed) return false;
            transform->local_position = *typed;
            return true;
        });

    property_access_.register_property(
        core::World::transform_type,
        "Local Rotation",
        core::PropertyKind::Quaternion,
        [](const core::World& world, core::Entity entity)
            -> std::optional<core::PropertyValue> {
            const auto* transform = world.transform(entity);
            if (!transform) return std::nullopt;
            return core::PropertyValue{transform->local_rotation};
        },
        [](core::World& world,
           core::Entity entity,
           const core::PropertyValue& value) {
            auto* transform = world.transform(entity);
            const auto* typed = std::get_if<core::Quat>(&value);
            if (!transform || !typed) return false;
            transform->local_rotation = *typed;
            return true;
        });

    property_access_.register_property(
        core::World::transform_type,
        "Local Scale",
        core::PropertyKind::Vec3,
        [](const core::World& world, core::Entity entity)
            -> std::optional<core::PropertyValue> {
            const auto* transform = world.transform(entity);
            if (!transform) return std::nullopt;
            return core::PropertyValue{transform->local_scale};
        },
        [](core::World& world,
           core::Entity entity,
           const core::PropertyValue& value) {
            auto* transform = world.transform(entity);
            const auto* typed = std::get_if<core::Vec3>(&value);
            if (!transform || !typed) return false;
            transform->local_scale = *typed;
            return true;
        });

    if (!register_render_integration(
            component_registry_,
            component_serialization_,
            property_access_)) {

        console_.warning(
            "Editor",
            "Render component integration was only partially registered.");
    }

    if (!register_render_component_factories(
            component_factories_)) {

        console_.warning(
            "Editor",
            "Render component factories were only partially registered.");
    }

    if (!register_physics_integration(
            component_registry_,
            component_serialization_,
            property_access_)) {

        console_.warning(
            "Editor",
            "Physics component integration was only partially registered.");
    }

    if (!register_physics_component_factories(
            component_factories_)) {

        console_.warning(
            "Editor",
            "Physics component factories were only partially registered.");
    }

    if (!register_audio_integration(
            component_registry_,
            component_serialization_,
            property_access_)) {

        console_.warning(
            "Editor",
            "Audio component integration was only partially registered.");
    }

    if (!register_audio_component_factories(
            component_factories_)) {

        console_.warning(
            "Editor",
            "Audio component factories were only partially registered.");
    }

    if (!register_scripting_integration(
            component_registry_,
            component_serialization_,
            property_access_)) {

        console_.warning(
            "Editor",
            "Scripting component integration was only partially registered.");
    }

    if (!register_scripting_component_factories(
            component_factories_)) {

        console_.warning(
            "Editor",
            "Scripting component factories were only partially registered.");
    }

    managed_script_system_.bind(
        &managed_runtime_);

    console_.info(
        "Editor",
        "EditorModel initialized.");
}


bool EditorModel::begin_play_mode() {
    if (!play_session_.play(
            world_)) {
        return false;
    }

    managed_script_system_.clear(play_session_.runtime_world());
    physics_contact_tracker_.clear();
    audio_playback_system_.reset();
    audio_mix_snapshot_ = {};

    std::string audio_device_error;

    if (!audio_output_device_.open(
            &audio_device_error)) {

        console_.warning(
            "Audio",
            audio_device_error.empty()
                ? "Audio output device could not be opened."
                : std::move(audio_device_error));
    } else {
        const auto info =
            audio_output_device_.info();

        if (!info.diagnostic.empty()) {
            console_.warning(
                "Audio",
                info.diagnostic);
        } else {
            console_.info(
                "Audio",
                "Audio output: " +
                    info.backend +
                    " " +
                    std::to_string(
                        info.sample_rate) +
                    " Hz, " +
                    std::to_string(
                        info.channels) +
                    " channel(s).");
        }
    }

    if (managed_runtime_.valid() &&
        !managed_runtime_.reset_time()) {

        console_.warning(
            "Scripting",
            managed_runtime_.diagnostic());
    }

    return true;
}

bool EditorModel::stop_play_mode() {
    managed_script_system_.clear(play_session_.runtime_world());
    physics_contact_tracker_.clear();
    audio_playback_system_.reset();
    audio_output_device_.close();
    audio_mix_snapshot_ = {};
    return play_session_.stop();
}

bool EditorModel::initialize_managed_runtime(
    const std::filesystem::path& hostfxr_path,
    const std::filesystem::path& runtime_config_path,
    const std::filesystem::path& assembly_path,
    std::string_view assembly_name,
    std::string* error) {

    managed_script_system_.clear(play_session_.runtime_world());
    physics_contact_tracker_.clear();
    audio_playback_system_.reset();
    audio_output_device_.close();
    audio_mix_snapshot_ = {};
    managed_runtime_.shutdown();

    if (!managed_runtime_.initialize(
            hostfxr_path,
            runtime_config_path,
            assembly_path,
            assembly_name)) {

        if (error) {
            *error =
                managed_runtime_
                    .diagnostic();
        }

        return false;
    }

    managed_runtime_.bind_input(
        &input_state_);

    bind_managed_property_access(
        managed_runtime_,
        property_access_);

    bind_managed_physics_queries(
        managed_runtime_);

    managed_script_system_.bind(
        &managed_runtime_);

    return true;
}

bool EditorModel::reload_managed_runtime(
    const std::filesystem::path& assembly_path,
    std::string_view assembly_name,
    std::string* error) {

    if (!managed_runtime_.valid()) {
        if (error) {
            *error =
                "managed runtime is not initialized";
        }

        return false;
    }

    // Managed instances hold types from the collectible gameplay context.
    // Destroy them before requesting unload; Play Mode World state remains.
    managed_script_system_.clear(play_session_.runtime_world());
    physics_contact_tracker_.clear();
    audio_playback_system_.reset();
    audio_mix_snapshot_ = {};

    if (!managed_runtime_.reload_gameplay(
            assembly_path,
            assembly_name)) {

        if (error) {
            *error =
                managed_runtime_
                    .diagnostic();
        }

        return false;
    }

    managed_runtime_.bind_input(
        &input_state_);

    bind_managed_property_access(
        managed_runtime_,
        property_access_);

    bind_managed_physics_queries(
        managed_runtime_);

    managed_script_system_.bind(
        &managed_runtime_);

    return true;
}

void EditorModel::shutdown_managed_runtime()
    noexcept {

    managed_script_system_.clear(play_session_.runtime_world());
    physics_contact_tracker_.clear();
    audio_playback_system_.reset();
    audio_output_device_.close();
    audio_mix_snapshot_ = {};
    managed_runtime_.shutdown();
}

void EditorModel::tick_runtime(
    double elapsed_seconds) {

    auto* runtime =
        play_session_.runtime_world();

    if (!runtime) {
        managed_script_system_.clear(
            play_session_.runtime_world());
        physics_contact_tracker_.clear();
        audio_playback_system_.reset();
        audio_output_device_.close();
        audio_mix_snapshot_ = {};
        return;
    }

    const auto steps =
        play_session_
            .consume_simulation_steps(
                elapsed_seconds);

    const auto play_state =
        play_session_.state();

    const bool run_playing_frame =
        play_state == PlayState::Playing &&
        elapsed_seconds > 0.0;

    if (steps == 0u &&
        !run_playing_frame) {
        return;
    }

    const auto fixed_delta =
        static_cast<float>(
            play_session_
                .fixed_delta_seconds());

    std::string animation_error;
    std::string script_error;
    std::string audio_error;
    std::string audio_device_error;

    const auto resolve_audio_duration =
        [this, &audio_error](
            assets::AssetGuid guid)
            -> std::optional<float> {

            const auto artifacts =
                project_.cached_artifacts(
                    guid);

            if (!artifacts) {
                return std::nullopt;
            }

            std::string load_error;

            const auto* clip =
                audio_clip_cache_.load(
                    guid,
                    *artifacts,
                    &load_error);

            if (!clip) {
                if (audio_error.empty()) {
                    audio_error =
                        std::move(load_error);
                }
                return std::nullopt;
            }

            return clip->duration_seconds();
        };

    if (managed_runtime_.valid()) {
        managed_runtime_.bind_input(
            &input_state_);
    }

    const auto run_fixed_step =
        [&]() {
            render::update_sprite_animators(
                *runtime,
                fixed_delta,
                sprite_animation_cache_,
                [this](
                    assets::AssetGuid guid) {
                    return project_
                        .cached_artifacts(
                            guid);
                },
                &animation_error);

            if (managed_runtime_.valid()) {
                managed_script_system_
                    .fixed_update(
                        *runtime,
                        fixed_delta,
                        &script_error);
            }

            const auto physics_frame =
                physics::step_physics(
                    *runtime,
                    fixed_delta);

            const auto contact_events =
                physics_contact_tracker_.update(
                    physics_frame
                        .collisions
                        .overlaps);

            if (managed_runtime_.valid()) {
                for (const auto& event :
                     contact_events) {

                    const int phase =
                        static_cast<int>(
                            event.phase);

                    if (!managed_script_system_
                            .dispatch_physics_event(
                                *runtime,
                                event.first,
                                event.second,
                                phase,
                                event.is_trigger,
                                event.is_2d,
                                event.normal,
                                event.penetration,
                                &script_error)) {
                        continue;
                    }

                    managed_script_system_
                        .dispatch_physics_event(
                            *runtime,
                            event.second,
                            event.first,
                            phase,
                            event.is_trigger,
                            event.is_2d,
                            {
                                -event.normal.x,
                                -event.normal.y,
                                -event.normal.z
                            },
                            event.penetration,
                            &script_error);
                }
            }
        };

    if (play_state ==
        PlayState::Paused) {

        // Each requested editor Step is one complete simulation frame.
        for (std::uint32_t step = 0u;
             step < steps;
             ++step) {

            run_fixed_step();

            if (managed_runtime_.valid()) {
                managed_script_system_.update(
                    *runtime,
                    fixed_delta,
                    &script_error);
            }

            audio_playback_system_.update(
                *runtime,
                fixed_delta,
                resolve_audio_duration);

            audio_mix_snapshot_ =
                audio::build_mix_snapshot(
                    *runtime);

            if (audio_output_device_.is_open()) {
                audio_output_device_.pump(
                    audio_mix_snapshot_,
                    [this](assets::AssetGuid guid) {
                        return audio_clip_cache_.find(
                            guid);
                    },
                    &audio_device_error);
            }
        }
    } else {
        // A host frame may contain zero or several fixed simulation steps.
        for (std::uint32_t step = 0u;
             step < steps;
             ++step) {

            run_fixed_step();
        }

        if (run_playing_frame) {

            const auto clamped_elapsed =
                elapsed_seconds > 0.25
                    ? 0.25
                    : elapsed_seconds;

            const auto frame_delta =
                static_cast<float>(
                    clamped_elapsed);

            if (managed_runtime_.valid()) {
                managed_script_system_.update(
                    *runtime,
                    frame_delta,
                    &script_error);
            }

            audio_playback_system_.update(
                *runtime,
                frame_delta,
                resolve_audio_duration);

            audio_mix_snapshot_ =
                audio::build_mix_snapshot(
                    *runtime);

            if (audio_output_device_.is_open()) {
                audio_output_device_.pump(
                    audio_mix_snapshot_,
                    [this](assets::AssetGuid guid) {
                        return audio_clip_cache_.find(
                            guid);
                    },
                    &audio_device_error);
            }
        }
    }

    if (!script_error.empty()) {
        console_.warning(
            "Scripting",
            std::move(
                script_error));
    }

    if (!animation_error.empty()) {
        console_.warning(
            "Animation",
            std::move(
                animation_error));
    }

    if (!audio_error.empty()) {
        console_.warning(
            "Audio",
            std::move(
                audio_error));
    }

    if (!audio_device_error.empty()) {
        console_.warning(
            "Audio",
            std::move(
                audio_device_error));
    }
}

} // namespace nengine::editor

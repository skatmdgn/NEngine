#include "nengine/editor/editor_model.hpp"
#include "nengine/editor/render_integration.hpp"
#include "nengine/editor/scripting_integration.hpp"

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
    return play_session_.stop();
}

bool EditorModel::initialize_managed_runtime(
    const std::filesystem::path& hostfxr_path,
    const std::filesystem::path& runtime_config_path,
    const std::filesystem::path& assembly_path,
    std::string_view assembly_name,
    std::string* error) {

    managed_script_system_.clear(play_session_.runtime_world());
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

    managed_script_system_.bind(
        &managed_runtime_);

    return true;
}

void EditorModel::shutdown_managed_runtime()
    noexcept {

    managed_script_system_.clear(play_session_.runtime_world());
    managed_runtime_.shutdown();
}

void EditorModel::tick_runtime(
    double elapsed_seconds) {

    auto* runtime =
        play_session_.runtime_world();

    if (!runtime) {
        managed_script_system_.clear(
            play_session_.runtime_world());
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
        }
    } else {
        // A host frame may contain zero or several fixed simulation steps.
        for (std::uint32_t step = 0u;
             step < steps;
             ++step) {

            run_fixed_step();
        }

        if (managed_runtime_.valid() &&
            run_playing_frame) {

            const auto clamped_elapsed =
                elapsed_seconds > 0.25
                    ? 0.25
                    : elapsed_seconds;

            managed_script_system_.update(
                *runtime,
                static_cast<float>(
                    clamped_elapsed),
                &script_error);
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
}

} // namespace nengine::editor

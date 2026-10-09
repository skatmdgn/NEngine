#include "nengine/editor/editor_model.hpp"
#include "nengine/editor/render_integration.hpp"
#include "nengine/editor/scripting_integration.hpp"

namespace nengine::editor {

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


bool EditorModel::initialize_managed_runtime(
    const std::filesystem::path& hostfxr_path,
    const std::filesystem::path& runtime_config_path,
    const std::filesystem::path& assembly_path,
    std::string_view assembly_name,
    std::string* error) {

    managed_script_system_.clear();
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

    managed_script_system_.bind(
        &managed_runtime_);

    return true;
}

void EditorModel::shutdown_managed_runtime()
    noexcept {

    managed_script_system_.clear();
    managed_runtime_.shutdown();
}

void EditorModel::tick_runtime(
    double elapsed_seconds) {

    auto* runtime =
        play_session_.runtime_world();

    if (!runtime) {
        managed_script_system_.clear();
        return;
    }

    const auto steps =
        play_session_
            .consume_simulation_steps(
                elapsed_seconds);

    if (steps == 0u) {
        return;
    }

    std::string animation_error;

    for (std::uint32_t step = 0u;
         step < steps;
         ++step) {

        render::update_sprite_animators(
            *runtime,
            static_cast<float>(
                play_session_
                    .fixed_delta_seconds()),
            sprite_animation_cache_,
            [this](
                assets::AssetGuid guid) {
                return project_
                    .cached_artifacts(
                        guid);
            },
            &animation_error);


        if (managed_runtime_.valid()) {
            std::string script_error;

            managed_script_system_.update(
                *runtime,
                static_cast<float>(
                    play_session_
                        .fixed_delta_seconds()),
                &script_error);

            if (!script_error.empty()) {
                console_.warning(
                    "Scripting",
                    std::move(
                        script_error));
            }
        }
    }

    if (!animation_error.empty()) {
        console_.warning(
            "Animation",
            std::move(
                animation_error));
    }
}

} // namespace nengine::editor

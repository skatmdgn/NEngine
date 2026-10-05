#include "nengine/editor/editor_model.hpp"

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

    console_.info(
        "Editor",
        "EditorModel initialized.");
}

} // namespace nengine::editor

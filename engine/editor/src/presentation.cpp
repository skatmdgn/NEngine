#include "nengine/editor/presentation.hpp"

#include <string>
#include <unordered_set>

namespace nengine::editor {
namespace {

void append_branch(
    const core::World& world,
    const Selection& selection,
    core::Entity entity,
    std::uint32_t depth,
    std::unordered_set<core::Entity::value_type>& visited,
    std::vector<HierarchyRow>& rows) {

    if (!world.is_alive(entity) ||
        !visited.insert(entity.value).second) {
        return;
    }

    rows.push_back({
        entity,
        std::string{world.name(entity)},
        depth,
        world.active(entity),
        selection.active() == entity
    });

    for (const auto child : world.children(entity)) {
        append_branch(
            world,
            selection,
            child,
            depth + 1,
            visited,
            rows);
    }
}

InspectorComponent build_component(
    const EditorModel& editor,
    const core::World& world,
    core::Entity entity,
    core::ComponentTypeId type) {

    InspectorComponent component;
    component.type = type;

    const auto* descriptor =
        editor.component_registry().find(type);

    if (!descriptor) {
        component.name = "Unknown Component";
        return component;
    }

    component.name = descriptor->name;

    for (const auto& property :
         descriptor->properties) {

        InspectorField field;
        field.label = property.name;
        field.property_path = property.name;
        field.kind = property.kind;

        const auto value =
            editor.property_access().read(
                world,
                entity,
                type,
                property.name);

        if (value) {
            field.value = *value;
        }

        field.editable =
            editor.can_edit() &&
            value.has_value() &&
            core::has_flag(
                property.flags,
                core::PropertyFlags::Editable) &&
            !core::has_flag(
                property.flags,
                core::PropertyFlags::ReadOnly);

        if (!core::has_flag(
                property.flags,
                core::PropertyFlags::Hidden)) {
            component.fields.push_back(
                std::move(field));
        }
    }

    return component;
}

} // namespace

std::vector<HierarchyRow> build_hierarchy(
    const EditorModel& editor) {

    std::vector<HierarchyRow> rows;
    std::unordered_set<
        core::Entity::value_type> visited;

    const auto& world =
        editor.presentation_world();

    const auto entities =
        world.entities();

    rows.reserve(entities.size());
    visited.reserve(entities.size());

    for (const auto entity : entities) {
        const auto* transform =
            world.transform(entity);

        if (transform &&
            !transform->parent.valid()) {
            append_branch(
                world,
                editor.selection(),
                entity,
                0,
                visited,
                rows);
        }
    }

    for (const auto entity : entities) {
        if (!visited.contains(entity.value)) {
            append_branch(
                world,
                editor.selection(),
                entity,
                0,
                visited,
                rows);
        }
    }

    return rows;
}

InspectorSnapshot build_inspector(
    const EditorModel& editor) {

    InspectorSnapshot snapshot;

    const auto entity =
        editor.selection().active();

    const auto& world =
        editor.presentation_world();

    if (!world.is_alive(entity)) {
        return snapshot;
    }

    snapshot.valid = true;
    snapshot.entity = entity;
    snapshot.name =
        std::string{world.name(entity)};
    snapshot.active =
        world.active(entity);

    for (const auto type :
         world.component_types(entity)) {

        snapshot.components.push_back(
            build_component(
                editor,
                world,
                entity,
                type));
    }

    return snapshot;
}

std::optional<std::size_t> find_inspector_property_row(
    const InspectorSnapshot& snapshot,
    core::ComponentTypeId component,
    std::string_view property_path) {

    if (!snapshot.valid) {
        return std::nullopt;
    }

    std::size_t row = 0;

    for (const auto& entry : snapshot.components) {
        // The Win32 Reflection Properties list excludes Transform.
        if (entry.type == core::World::transform_type) {
            continue;
        }

        for (const auto& field : entry.fields) {
            if (entry.type == component &&
                field.property_path == property_path) {
                return row;
            }
            ++row;
        }
    }

    return std::nullopt;
}

ToolbarState build_toolbar(
    const EditorModel& editor) {

    ToolbarState state;

    const auto play_state =
        editor.play_session().state();

    state.can_play =
        play_state == PlayState::Editing;

    state.can_stop =
        play_state != PlayState::Editing;

    state.can_pause =
        play_state == PlayState::Playing;

    state.can_resume =
        play_state == PlayState::Paused;

    state.can_step =
        play_state == PlayState::Paused;

    state.can_undo =
        editor.can_edit() &&
        editor.commands().can_undo();

    state.can_redo =
        editor.can_edit() &&
        editor.commands().can_redo();

    if (state.can_undo) {
        state.undo_label =
            "Undo " +
            std::string{
                editor.commands().undo_name()
            };
    }

    if (state.can_redo) {
        state.redo_label =
            "Redo " +
            std::string{
                editor.commands().redo_name()
            };
    }

    return state;
}

} // namespace nengine::editor

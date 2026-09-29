#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "nengine/core/component_registry.hpp"
#include "nengine/core/property_value.hpp"
#include "nengine/editor/editor_model.hpp"

namespace nengine::editor {

struct HierarchyRow {
    core::Entity entity{core::Entity::invalid()};
    std::string name{};
    std::uint32_t depth{0};
    bool active{true};
    bool selected{false};
};

struct InspectorField {
    std::string label{};
    std::string property_path{};
    core::PropertyKind kind{core::PropertyKind::String};
    core::PropertyValue value{};
    bool editable{true};
};

struct InspectorComponent {
    core::ComponentTypeId type{core::ComponentRegistry::invalid_type};
    std::string name{};
    std::vector<InspectorField> fields{};
};

struct InspectorSnapshot {
    bool valid{false};
    core::Entity entity{core::Entity::invalid()};
    std::string name{};
    bool active{true};
    std::vector<InspectorComponent> components{};
};

struct ToolbarState {
    bool can_play{false};
    bool can_stop{false};
    bool can_pause{false};
    bool can_resume{false};
    bool can_step{false};
    bool can_undo{false};
    bool can_redo{false};
    std::string undo_label{"Undo"};
    std::string redo_label{"Redo"};
};

std::vector<HierarchyRow> build_hierarchy(const EditorModel& editor);
InspectorSnapshot build_inspector(const EditorModel& editor);
ToolbarState build_toolbar(const EditorModel& editor);

} // namespace nengine::editor

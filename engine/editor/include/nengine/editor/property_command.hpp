#pragma once

#include <optional>
#include <string>
#include <utility>

#include "nengine/editor/command.hpp"
#include "nengine/editor/property_access.hpp"

namespace nengine::editor {

class SetPropertyCommand final : public EditorCommand {
public:
    SetPropertyCommand(
        const PropertyAccessRegistry* access,
        core::Entity entity,
        core::ComponentTypeId component,
        std::string property_name,
        core::PropertyValue new_value)
        : access_(access),
          entity_(entity),
          component_(component),
          property_name_(std::move(property_name)),
          new_value_(std::move(new_value)) {}

    bool execute(core::World& world) override;
    void undo(core::World& world) override;
    std::string_view name() const noexcept override {
        return "Set Property";
    }

private:
    const PropertyAccessRegistry* access_{nullptr};
    core::Entity entity_{core::Entity::invalid()};
    core::ComponentTypeId component_{
        core::ComponentRegistry::invalid_type};
    std::string property_name_{};
    core::PropertyValue old_value_{};
    core::PropertyValue new_value_{};
    bool captured_{false};
};

} // namespace nengine::editor

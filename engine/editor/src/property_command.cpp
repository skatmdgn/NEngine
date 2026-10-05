#include "nengine/editor/property_command.hpp"

namespace nengine::editor {

bool SetPropertyCommand::execute(
    core::World& world) {

    if (!access_ ||
        !world.is_alive(entity_)) {
        return false;
    }

    if (!captured_) {
        const auto old =
            access_->read(
                world,
                entity_,
                component_,
                property_name_);

        if (!old) return false;
        old_value_ = *old;
        captured_ = true;
    }

    return access_->write(
        world,
        entity_,
        component_,
        property_name_,
        new_value_);
}

void SetPropertyCommand::undo(
    core::World& world) {

    if (!captured_ ||
        !access_ ||
        !world.is_alive(entity_)) {
        return;
    }

    access_->write(
        world,
        entity_,
        component_,
        property_name_,
        old_value_);
}

} // namespace nengine::editor

#include "nengine/editor/command.hpp"

namespace nengine::editor {

bool CommandStack::execute(core::World& world, std::unique_ptr<EditorCommand> command) {
    if (!command || !command->execute(world)) return false;

    if (cursor_ < history_.size()) {
        history_.erase(
            history_.begin() +
                static_cast<std::ptrdiff_t>(cursor_),
            history_.end());

        state_ids_.erase(
            state_ids_.begin() +
                static_cast<std::ptrdiff_t>(cursor_ + 1),
            state_ids_.end());
    }

    history_.push_back(std::move(command));
    ++cursor_;
    state_ids_.push_back(next_state_id_++);
    return true;
}

bool CommandStack::undo(core::World& world) {
    if (!can_undo()) return false;
    --cursor_;
    history_[cursor_]->undo(world);
    return true;
}

bool CommandStack::redo(core::World& world) {
    if (!can_redo()) return false;
    if (!history_[cursor_]->execute(world)) return false;
    ++cursor_;
    return true;
}

void CommandStack::clear() noexcept {
    history_.clear();
    cursor_ = 0;
    state_ids_.clear();
    state_ids_.push_back(next_state_id_++);
}

std::string_view CommandStack::undo_name() const noexcept {
    if (!can_undo()) return {};
    return history_[cursor_ - 1]->name();
}

std::string_view CommandStack::redo_name() const noexcept {
    if (!can_redo()) return {};
    return history_[cursor_]->name();
}

bool CreateEntityCommand::execute(
    core::World& world) {

    if (parent_.valid() &&
        !world.is_alive(parent_)) {
        return false;
    }

    created_ =
        world.create(object_name_);

    if (parent_.valid() &&
        !world.set_parent(
            created_,
            parent_)) {

        world.destroy(created_);
        created_ =
            core::Entity::invalid();
        return false;
    }

    return true;
}

void CreateEntityCommand::undo(
    core::World& world) {

    if (world.is_alive(created_)) {
        world.destroy(created_);
    }
}

bool DeleteEntityCommand::execute(
    core::World& world) {

    if (!world.is_alive(entity_)) {
        return false;
    }

    if (!before_) {
        before_.emplace(world.clone());
    }

    std::vector<core::Entity> stack{
        entity_
    };

    std::vector<core::Entity> ordered;

    while (!stack.empty()) {
        const auto current =
            stack.back();
        stack.pop_back();

        if (!world.is_alive(current)) {
            continue;
        }

        ordered.push_back(current);

        for (const auto child :
             world.children(current)) {
            stack.push_back(child);
        }
    }

    for (auto it = ordered.rbegin();
         it != ordered.rend();
         ++it) {
        world.destroy(*it);
    }

    return true;
}

void DeleteEntityCommand::undo(
    core::World& world) {

    if (before_) {
        world = before_->clone();
    }
}

bool RenameEntityCommand::execute(core::World& world) {
    if (!world.is_alive(entity_) || new_name_.empty()) return false;
    if (!captured_) {
        old_name_ = std::string{world.name(entity_)};
        captured_ = true;
    }
    return world.set_name(entity_, new_name_);
}

void RenameEntityCommand::undo(core::World& world) {
    if (captured_ && world.is_alive(entity_)) world.set_name(entity_, old_name_);
}

bool SetActiveCommand::execute(core::World& world) {
    if (!world.is_alive(entity_)) return false;
    if (!captured_) {
        old_value_ = world.active(entity_);
        captured_ = true;
    }
    return world.set_active(entity_, new_value_);
}

void SetActiveCommand::undo(core::World& world) {
    if (captured_ && world.is_alive(entity_)) world.set_active(entity_, old_value_);
}

bool SetTransformCommand::execute(core::World& world) {
    auto* transform = world.transform(entity_);
    if (!transform) return false;
    if (!captured_) {
        old_value_ = *transform;
        captured_ = true;
    }
    const auto requested_parent = new_value_.parent;
    if (!world.set_parent(entity_, requested_parent)) return false;
    *transform = new_value_;
    return true;
}

void SetTransformCommand::undo(core::World& world) {
    auto* transform = world.transform(entity_);
    if (!captured_ || !transform) return;
    if (!world.set_parent(entity_, old_value_.parent)) return;
    *transform = old_value_;
}

} // namespace nengine::editor

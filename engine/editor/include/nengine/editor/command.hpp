#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "nengine/core/transform.hpp"
#include "nengine/core/world.hpp"

namespace nengine::editor {

class EditorCommand {
public:
    virtual ~EditorCommand() = default;
    virtual bool execute(core::World& world) = 0;
    virtual void undo(core::World& world) = 0;
    virtual std::string_view name() const noexcept = 0;
};

class CommandStack {
public:
    bool execute(core::World& world, std::unique_ptr<EditorCommand> command);
    bool undo(core::World& world);
    bool redo(core::World& world);
    void clear() noexcept;

    bool can_undo() const noexcept { return cursor_ > 0; }
    bool can_redo() const noexcept { return cursor_ < history_.size(); }
    std::size_t size() const noexcept { return history_.size(); }
    std::size_t cursor() const noexcept { return cursor_; }
    std::uint64_t state_id() const noexcept {
        return state_ids_[cursor_];
    }
    std::string_view undo_name() const noexcept;
    std::string_view redo_name() const noexcept;

private:
    std::vector<std::unique_ptr<EditorCommand>> history_{};
    std::vector<std::uint64_t> state_ids_{0};
    std::size_t cursor_{0};
    std::uint64_t next_state_id_{1};
};

class CreateEntityCommand final : public EditorCommand {
public:
    explicit CreateEntityCommand(
        std::string name = "GameObject",
        core::Entity parent = core::Entity::invalid())
        : object_name_(std::move(name)),
          parent_(parent) {}

    bool execute(core::World& world) override;
    void undo(core::World& world) override;
    std::string_view name() const noexcept override {
        return "Create Entity";
    }

    core::Entity created_entity() const noexcept {
        return created_;
    }

private:
    std::string object_name_{};
    core::Entity parent_{core::Entity::invalid()};
    core::Entity created_{core::Entity::invalid()};
};

class DeleteEntityCommand final : public EditorCommand {
public:
    explicit DeleteEntityCommand(core::Entity entity)
        : entity_(entity) {}

    bool execute(core::World& world) override;
    void undo(core::World& world) override;
    std::string_view name() const noexcept override {
        return "Delete Entity";
    }

private:
    core::Entity entity_{core::Entity::invalid()};
    std::optional<core::World> before_{};
};

class RenameEntityCommand final : public EditorCommand {
public:
    RenameEntityCommand(core::Entity entity, std::string new_name)
        : entity_(entity), new_name_(std::move(new_name)) {}

    bool execute(core::World& world) override;
    void undo(core::World& world) override;
    std::string_view name() const noexcept override { return "Rename Entity"; }

private:
    core::Entity entity_{};
    std::string old_name_{};
    std::string new_name_{};
    bool captured_{false};
};

class SetActiveCommand final : public EditorCommand {
public:
    SetActiveCommand(core::Entity entity, bool active)
        : entity_(entity), new_value_(active) {}

    bool execute(core::World& world) override;
    void undo(core::World& world) override;
    std::string_view name() const noexcept override { return "Set Active"; }

private:
    core::Entity entity_{};
    bool old_value_{true};
    bool new_value_{true};
    bool captured_{false};
};

class SetTransformCommand final : public EditorCommand {
public:
    SetTransformCommand(core::Entity entity, core::Transform value)
        : entity_(entity), new_value_(value) {}

    bool execute(core::World& world) override;
    void undo(core::World& world) override;
    std::string_view name() const noexcept override { return "Set Transform"; }

private:
    core::Entity entity_{};
    core::Transform old_value_{};
    core::Transform new_value_{};
    bool captured_{false};
};

} // namespace nengine::editor
